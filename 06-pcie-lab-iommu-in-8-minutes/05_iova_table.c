/* IOMMU in 8 Minutes: Translation and Isolation for Devices - slide 5: a model of the iova table (C version of 05_iova_table.cpp) */
/* Build: make 05_iova_table_c */
#if defined(__linux__)
#define _GNU_SOURCE
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct { uint64_t phys; bool write; } Entry;
typedef struct { uint64_t page; Entry e; } Mapping;          /* IOVA page -> entry */
typedef struct { Mapping map[8]; size_t n; } Domain;         /* C has no std::map: a small table */

static void map_page(Domain *d, uint64_t page, uint64_t phys, bool write) {
    if (d->n < sizeof d->map / sizeof d->map[0]) {
        d->map[d->n].page = page;
        d->map[d->n].e.phys = phys;
        d->map[d->n].e.write = write;
        ++d->n;
    }
}

/* One lookup per DMA: the device never sees a physical address.
   C has no std::optional: the result goes through *phys and the return says whether it exists. */
static bool translate(const Domain *d, uint64_t iova, bool wr, uint64_t *phys) {
    for (size_t i = 0; i < d->n; ++i) {
        if (d->map[i].page != (iova >> 12)) continue;       /* 4 KiB pages */
        if (wr && !d->map[i].e.write) return false;          /* read only */
        *phys = d->map[i].e.phys | (iova & 0xFFF);           /* frame plus offset */
        return true;
    }
    return false;                                            /* not mapped: fault */
}
/* NIC domain maps IOVA 0x1000 only: a DMA to 0x9000 is blocked */

/* One DMA of the model: prints what the IOMMU does with it and says whether that was the expected result. */
static bool dma(const char *bdf, const Domain *d, uint64_t iova, bool wr, bool expect_ok) {
    uint64_t phys = 0;
    const bool mapped = translate(d, iova, wr, &phys);
    printf("  %s%s IOVA 0x%" PRIx64, bdf, wr ? " DMA write" : " DMA read ", iova);
    if (mapped) {
        printf(" -> physical 0x%" PRIx64 "\n", phys);
    } else {
        printf(" -> BLOCKED\n");
        printf("    log (model): [%s] request device [%s] fault addr 0x%" PRIx64 "\n",
               wr ? "DMA Write" : "DMA Read", bdf, iova);
    }
    return mapped == expect_ok;
}

int main(void) {
    /* the driver of the NIC mapped one receive buffer, which the device writes */
    Domain nic = { .n = 0 };
    map_page(&nic, 0x1, 0x7f3000, true);
    /* and one transmit buffer, which the device only needs to read */
    map_page(&nic, 0x2, 0x7f5000, false);
    /* the NVMe drive has a domain of its own: the same IOVA leads somewhere else */
    Domain nvme = { .n = 0 };
    map_page(&nvme, 0x1, 0x2a0000, true);

    bool ok = true;
    printf("domain of the NIC (0000:03:00.0): 2 pages mapped\n");
    ok = dma("03:00.0", &nic, 0x1040, true, true) && ok;      /* inside the receive buffer: translated */
    ok = dma("03:00.0", &nic, 0x2010, false, true) && ok;     /* reads its transmit buffer: translated */
    ok = dma("03:00.0", &nic, 0x2010, true, false) && ok;     /* writes a read only page: blocked */
    ok = dma("03:00.0", &nic, 0x9000, true, false) && ok;     /* a stale descriptor: never mapped, blocked */
    printf("domain of the NVMe drive (0000:01:00.0): 1 page mapped\n");
    ok = dma("01:00.0", &nvme, 0x1040, true, true) && ok;     /* same IOVA, another domain, another frame */
    ok = dma("01:00.0", &nvme, 0x2010, false, false) && ok;   /* the buffers of the NIC do not exist here */

    uint64_t a = 0, b = 0;
    const bool has_a = translate(&nic, 0x1040, true, &a);
    const bool has_b = translate(&nvme, 0x1040, true, &b);
    ok = ok && has_a && has_b && a == 0x7f3040 && b == 0x2a0040;
    printf("same IOVA 0x1040, two domains, two frames: that is the isolation\n");
    printf("without an IOMMU every one of these six DMAs would have reached RAM\n");
    printf(ok ? "model check: OK\n" : "model check: FAILED\n");
    return ok ? 0 : 1;
}
