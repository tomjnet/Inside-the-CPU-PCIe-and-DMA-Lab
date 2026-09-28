/* DMA in 8 Minutes: Devices Move Data Themselves - slide 5: driver side: post buffers, ring the doorbell (C version of 05_post_buffers.cpp) */
/* Build: make 05_post_buffers_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* The model. Host RAM is an array of bytes. A DMA address is NOT a pointer: it is what the device puts on the
   bus. Here it is the offset into RAM plus a base, so mixing the two up is visible at once. A real driver gets
   it from the kernel (dma_map_single), and behind an IOMMU it is an I/O virtual address (next episode). */
#define RING 8u
#define BUF_SIZE 2048u
#define DMA_BASE 0x100000000ull

typedef struct {
    uint64_t dma_addr;
    uint16_t buf_len;
    uint16_t pkt_len;
    uint32_t status;
} RxDesc;

/* A device register that counts its writes: every write is one MMIO store, one PCIe crossing.
   C has no operator overloading, so the store goes through doorbell_write instead of "=". */
typedef struct {
    uint32_t value;
    int writes;
} Doorbell;
typedef struct { Doorbell rx_tail; } Mmio;

static void doorbell_write(Doorbell *d, uint32_t v) { d->value = v; ++d->writes; }

static uint8_t ram[RING * BUF_SIZE];
static RxDesc ring[RING];
static Mmio mmio;

static uint64_t dma_map(const uint8_t *p) {
    return DMA_BASE + (uint64_t)(p - ram);
}
static const uint8_t *ram_at(uint64_t dma_addr) {
    return ram + (size_t)(dma_addr - DMA_BASE);
}

int main(void) {
    uint8_t *buffers[RING];
    for (uint32_t i = 0; i < RING; ++i) buffers[i] = ram + (size_t)i * BUF_SIZE;
    uint32_t tail = 0;

    /* Driver: give every slot a pinned buffer, then ring the doorbell.
       Cost: one MMIO write, one PCIe crossing, for the whole batch. */
    for (uint32_t i = 0; i < RING; ++i) {
        ring[i].dma_addr = dma_map(buffers[i]);  /* pin, get bus address */
        ring[i].buf_len  = (uint16_t)BUF_SIZE;
        ring[i].status   = 0;                    /* not DONE yet */
    }
    tail = RING - 1;                             /* head == tail must mean "no buffer" */
    doorbell_write(&mmio.rx_tail, tail);         /* doorbell: slots below tail are yours */

    printf("slot  pointer (CPU, virtual)  dma_addr (device, bus)  buf_len  status\n");
    int ok = 1;
    for (uint32_t i = 0; i < RING; ++i) {
        printf("  %u   %p        0x%" PRIx64 "             %u     %u\n", i, (const void *)buffers[i],
               ring[i].dma_addr, (unsigned)ring[i].buf_len, (unsigned)ring[i].status);
        if (ram_at(ring[i].dma_addr) != buffers[i]) ok = 0;   /* the mapping must lead back to the buffer */
    }
    uint32_t usable = mmio.rx_tail.value;        /* head is 0: slots 0 to tail - 1 */
    printf("rx_tail register = %u: the device may fill %u of %u slots before it must drop\n",
           usable, usable, RING);
    printf("MMIO writes (PCIe crossings by the CPU) for %u buffers: %d\n", RING, mmio.rx_tail.writes);
    printf("pinned memory handed to the device: %u bytes\n", RING * BUF_SIZE);
    fputs(ok ? "every dma_addr maps back to its buffer\n" : "MISMATCH between dma_addr and buffer\n", stdout);
    return ok && mmio.rx_tail.writes == 1 ? 0 : 1;
}
