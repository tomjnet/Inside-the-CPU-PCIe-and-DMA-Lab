/* DMA in 8 Minutes: Devices Move Data Themselves - slide 8: scatter and gather (C version of 08_scatter_gather.cpp) */
/* Build: make 08_scatter_gather_c */
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
#include <string.h>

/* The model: host RAM is an array of 16 pages, a DMA address is an offset into it plus a base, and the device
   transmits by reading the pieces of the list out of RAM, one DMA read per piece, straight onto the wire. */
#define PAGE 4096u
#define DMA_BASE 0x100000000ull

static uint8_t ram[16 * PAGE];
static uint8_t wire[16 * PAGE];      /* what left the device */
static size_t wire_len = 0;
static int dma_reads = 0;

/* C has no templates: the entry type moves out of main so device_tx can name it */
typedef struct { uint64_t dma_addr; uint32_t len; } SgEntry;

static uint64_t dma_map(const uint8_t *p) {
    return DMA_BASE + (uint64_t)(p - ram);
}

static void device_tx(const SgEntry *sg, size_t n) {
    wire_len = 0;
    for (size_t i = 0; i < n; ++i) {  /* gather: the device walks the list, the CPU does nothing */
        const uint8_t *from = ram + (size_t)(sg[i].dma_addr - DMA_BASE);
        memcpy(wire + wire_len, from, sg[i].len);
        wire_len += sg[i].len;
        ++dma_reads;
    }
}

int main(void) {
    /* Three buffers in three unrelated places of RAM, as the kernel would find them. */
    uint8_t *headers = ram + 11 * PAGE + 64;   /* built by the network stack */
    uint8_t *page_a  = ram + 7 * PAGE;         /* user payload starts near the end of this page */
    uint8_t *page_b  = ram + 2 * PAGE;         /* and continues here: not adjacent, not even in order */
    for (size_t i = 0; i < 54; ++i) headers[i] = (uint8_t)(0xA0 + i % 16);
    for (size_t i = 0; i < 400; ++i) page_a[3696 + i] = (uint8_t)(i % 251);
    for (size_t i = 0; i < 1060; ++i) page_b[i] = (uint8_t)((400 + i) % 251);

    /* Scatter/gather: one frame from three buffers, zero memcpy. */
    SgEntry sg[3] = {
        { dma_map(headers),       54 },    /* Ethernet, IP, TCP: the stack */
        { dma_map(page_a) + 3696, 400 },   /* user payload, end of a page */
        { dma_map(page_b),        1060 },  /* and the start of the next */
    };
    device_tx(sg, 3);   /* the device gathers 3 pieces into 1 frame */

    printf("scatter/gather list handed to the device:\n");
    for (size_t i = 0; i < 3; ++i)
        printf("  dma_addr=0x%" PRIx64 "  len=%u\n", sg[i].dma_addr, (unsigned)sg[i].len);

    /* What a driver without scatter/gather would do: copy everything into one bounce buffer first. */
    static uint8_t expected[1514];
    size_t expected_len = 0;
    memcpy(expected + expected_len, headers, 54);        expected_len += 54;
    memcpy(expected + expected_len, page_a + 3696, 400); expected_len += 400;
    memcpy(expected + expected_len, page_b, 1060);       expected_len += 1060;

    int ok = wire_len == expected_len && memcmp(wire, expected, expected_len) == 0 && wire_len == 1514 &&
             dma_reads == 3;
    printf("frame on the wire: %zu bytes from %d DMA reads\n", wire_len, dma_reads);
    printf("bytes copied by the CPU with the list: 0; with one bounce buffer: %zu\n", expected_len);
    printf("payload is continuous across the two pages: byte 453 = %d, byte 454 = %d\n",
           (int)wire[453], (int)wire[454]);
    fputs(ok ? "every byte of the frame matches\n" : "MISMATCH in the gathered frame\n", stdout);
    return ok ? 0 : 1;
}
