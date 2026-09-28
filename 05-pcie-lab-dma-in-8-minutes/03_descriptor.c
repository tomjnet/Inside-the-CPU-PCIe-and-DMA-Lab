/* DMA in 8 Minutes: Devices Move Data Themselves - slide 3: pinned buffers, dma addresses, descriptors (C version of 03_descriptor.cpp) */
/* Build: make 03_descriptor_c */
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

#define RING 8u                      /* a real NIC ring holds 256 to 4096 descriptors */
#define DONE 1u

/* One RX descriptor: what the device reads to find a buffer. */
typedef struct {
    uint64_t dma_addr;               /* bus address of a pinned buffer */
    uint16_t buf_len;                /* room in that buffer, in bytes */
    uint16_t pkt_len;                /* written by the device: bytes used */
    uint32_t status;                 /* bit 0 = DONE, written by the device */
} RxDesc;
_Static_assert(sizeof(RxDesc) == 16, "4 descriptors per cache line");

int main(void) {
    RxDesc ring[RING];               /* coherent memory, shared */

    /* The driver writes the first two fields, the device writes the last two. */
    for (uint32_t i = 0; i < RING; ++i) {
        RxDesc d = { 0x100000000ull + i * 2048ull, 2048, 0, 0 };
        ring[i] = d;
    }
    ring[0].pkt_len = 1514;          /* what the device would write back */
    ring[0].status = DONE;

    printf("RxDesc layout (the device and the driver must agree on every byte)\n");
    printf("  field      offset  size  written by\n");
    printf("  dma_addr   %zu       8     driver\n", offsetof(RxDesc, dma_addr));
    printf("  buf_len    %zu       2     driver\n", offsetof(RxDesc, buf_len));
    printf("  pkt_len    %zu      2     device\n", offsetof(RxDesc, pkt_len));
    printf("  status     %zu      4     device\n", offsetof(RxDesc, status));
    printf("sizeof(RxDesc) = %zu bytes, %zu descriptors per 64 byte cache line\n",
           sizeof(RxDesc), 64 / sizeof(RxDesc));
    printf("this ring: %u slots = %zu bytes\n", RING, sizeof ring);
    printf("a 512 slot ring = %zu bytes, a 4096 slot ring = %zu bytes of coherent memory\n",
           512 * sizeof(RxDesc), 4096 * sizeof(RxDesc));

    printf("slot 0 after one packet: dma_addr=0x%" PRIx64 " buf_len=%u pkt_len=%u DONE=%s\n",
           ring[0].dma_addr, (unsigned)ring[0].buf_len, (unsigned)ring[0].pkt_len,
           (ring[0].status & DONE) ? "yes" : "no");
    printf("slot 1, still waiting:   dma_addr=0x%" PRIx64 " DONE=%s\n",
           ring[1].dma_addr, (ring[1].status & DONE) ? "yes" : "no");
    return 0;
}
