/* DMA in 8 Minutes: Devices Move Data Themselves - slide 6: device side: a packet arrives (C version of 06_device_rx.cpp) */
/* Build: make 06_device_rx_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* The model: host RAM is an array, a DMA address is an offset into it plus a base (never a pointer), and the
   device is the function below. On real hardware that function is silicon: no CPU instruction runs for it. */
#define RING 8u
#define BUF_SIZE 2048u
#define DMA_BASE 0x100000000ull
#define DONE 1u
#define MAX_PKT 1514u

typedef struct {
    uint64_t dma_addr;
    uint16_t buf_len;
    uint16_t pkt_len;
    uint32_t status;
} RxDesc;
typedef struct { uint32_t rx_tail; } Mmio;

static uint8_t ram[RING * BUF_SIZE];
static RxDesc ring[RING];
static Mmio mmio;
static uint32_t head = 0;            /* device register: next slot the device fills */
static long long dma_bytes = 0;      /* bytes the device wrote into host RAM by itself */

/* fixed-seed splitmix64 in place of std::mt19937: the packet sizes differ from the C++ run */
static uint64_t rng_state = 42;
static uint32_t rng_next(void) {
    uint64_t z = (rng_state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return (uint32_t)((z ^ (z >> 31)) >> 32);
}

static void dma_write(uint64_t dma_addr, const uint8_t *data, size_t len) {
    size_t offset = (size_t)(dma_addr - DMA_BASE);
    if (offset + len > sizeof ram) return;   /* a real device would corrupt memory here: see the IOMMU */
    memcpy(ram + offset, data, len);
    dma_bytes += (long long)len;
}

/* Device: a packet arrived on the wire. The CPU runs nothing here. */
static int device_rx(const uint8_t *pkt, size_t len) {
    if (head == mmio.rx_tail) return 0;      /* no buffer: drop */
    RxDesc *d = &ring[head];                 /* DMA read, 16 bytes */
    dma_write(d->dma_addr, pkt, len);        /* DMA write, the payload */
    d->pkt_len = (uint16_t)len;
    d->status  = DONE;                       /* ownership: driver */
    head = (head + 1) % RING;
    return 1;  /* then one MSI-X interrupt, or none when polled */
}

int main(void) {
    for (uint32_t i = 0; i < RING; ++i) {
        RxDesc d = { DMA_BASE + i * (uint64_t)BUF_SIZE, (uint16_t)BUF_SIZE, 0, 0 };
        ring[i] = d;
    }
    mmio.rx_tail = RING - 1;                 /* the driver posted the buffers (slide 5) */

    static uint8_t pkt[MAX_PKT];
    int accepted = 0;
    int dropped = 0;
    printf("9 packets arrive and the driver never harvests:\n");
    for (int p = 0; p < 9; ++p) {
        size_t len = 60 + rng_next() % 1455;                     /* 60 to 1514 bytes */
        for (size_t i = 0; i < len; ++i) pkt[i] = (uint8_t)(rng_next() & 0xFFu);
        uint32_t slot = head;
        int ok = device_rx(pkt, len);
        if (ok) {
            ++accepted;
            const uint8_t *in_ram = ram + (size_t)slot * BUF_SIZE;
            int same = memcmp(pkt, in_ram, len) == 0;
            printf("  packet %d: %zu bytes, slot %u, pkt_len=%u, DONE=%u, bytes in host RAM %s, head=%u\n",
                   p, len, slot, (unsigned)ring[slot].pkt_len, (unsigned)(ring[slot].status & DONE),
                   same ? "match" : "DIFFER", head);
            if (!same) return 1;
        } else {
            ++dropped;
            printf("  packet %d: %zu bytes, DROPPED: head == tail == %u (the rx_no_buffer counter of a real NIC)\n",
                   p, len, head);
        }
    }
    printf("accepted %d, dropped %d, %lld bytes moved by the device, 0 by the CPU\n", accepted, dropped, dma_bytes);
    printf("a ring of %u slots holds %u packets: one slot stays out so that full and empty differ\n",
           RING, RING - 1);
    return accepted == 7 && dropped == 2 ? 0 : 1;
}
