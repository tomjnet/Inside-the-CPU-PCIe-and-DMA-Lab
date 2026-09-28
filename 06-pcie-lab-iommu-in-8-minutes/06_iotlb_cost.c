/* IOMMU in 8 Minutes: Translation and Isolation for Devices - slide 6: the cost: iotlb, map and unmap (C version of 06_iotlb_cost.cpp) */
/* Build: make 06_iotlb_cost_c */
#if defined(__linux__)
#define _GNU_SOURCE
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define IOTLB_ENTRIES 8

typedef struct {                       /* the cache inside the IOMMU */
    uint64_t tlb[IOTLB_ENTRIES];       /* 8 entries, 0 means empty */
    size_t next, dmas, walks;
} Iotlb;

static void iotlb_access(Iotlb *t, uint64_t iova) {
    const uint64_t p = (iova >> 12) + 1;
    ++t->dmas;
    for (size_t i = 0; i < IOTLB_ENTRIES; ++i)
        if (t->tlb[i] == p) return;
    ++t->walks;                                   /* miss: page walk in RAM */
    t->tlb[t->next++ % IOTLB_ENTRIES] = p;        /* replace round robin */
}

static void iotlb_unmap(Iotlb *t) { memset(t->tlb, 0, sizeof t->tlb); }   /* invalidation: all cold again */

/* A ring of `buffers` pages, one DMA per buffer, `passes` times around the ring.
   unmap_each_pass plays a driver that unmaps its buffers after every pass instead of recycling them. */
static Iotlb play(uint64_t buffers, size_t passes, bool unmap_each_pass) {
    Iotlb t;
    memset(&t, 0, sizeof t);
    for (size_t pass = 0; pass < passes; ++pass) {
        for (uint64_t b = 0; b < buffers; ++b) iotlb_access(&t, 0x100000 + b * 4096);
        if (unmap_each_pass) iotlb_unmap(&t);
    }
    return t;
}

static void report(const char *what, const Iotlb *t) {
    const double walk_ns = 250.0;      /* a typical page walk of a few memory reads, not a measurement */
    printf("  %s: %zu DMAs, %zu page walks, about %g microseconds of walks (typical cost)\n",
           what, t->dmas, t->walks, (double)t->walks * walk_ns / 1000.0);
}

int main(void) {
    const size_t passes = 1000;
    const Iotlb hot = play(8, passes, false);        /* 8 buffers kept mapped: they fit in the IOTLB */
    const Iotlb wide = play(16, passes, false);      /* 16 buffers, 8 entries: the ring evicts itself */
    const Iotlb churn = play(8, passes, true);       /* 8 buffers, unmapped and mapped again every pass */

    printf("IOTLB model: 8 entries, 4 KiB pages, %zu passes around the ring\n", passes);
    report("8 buffers, mapped once and recycled ", &hot);
    report("16 buffers, mapped once and recycled", &wide);
    report("8 buffers, unmap after every pass   ", &churn);
    printf("the unmap itself also costs CPU time: a command to the IOMMU and a wait, per invalidation\n");
    printf("a real IOTLB is larger, and a 2 MiB page covers 512 times more per entry: same rule, other numbers\n");

    const bool ok = hot.walks == 8 && wide.walks == 16 * passes && churn.walks == 8 * passes;
    printf(ok ? "model check: OK\n" : "model check: FAILED\n");
    return ok ? 0 : 1;
}
