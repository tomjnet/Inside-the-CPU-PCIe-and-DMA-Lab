/* BARs and MMIO in 8 Minutes: Talking to a Device - slide 3: what a bar asks for (C version of 03_bar_sizing.cpp) */
/* Build: make 03_bar_sizing_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* A toy BAR, as the device implements it: the low address bits are hard wired to zero, so a write of
   all ones reads back as a mask. size must be a power of two; flags are the read only low bits.
   C has no member functions, so write and read take the BAR as a pointer. */
typedef struct {
    uint32_t size;
    uint32_t flags;
    uint32_t value;
} ToyBar;

static void bar_write(ToyBar *b, uint32_t v) { b->value = (v & ~(b->size - 1)) | b->flags; }
static uint32_t bar_read(const ToyBar *b) { return b->value; }

static void decode(const char *name, uint32_t readback) {
    bool io = (readback & 0x1) != 0;
    uint32_t mask = io ? (readback & ~0x3u) : (readback & ~0xFu);   /* an I/O BAR has only 2 flag bits */
    uint32_t bytes = ~mask + 1;
    printf("  %-16s read back 0x%08" PRIx32 "  %s  size %9" PRIu32 " bytes", name, readback,
           io ? "I/O space   " : "memory space", bytes);
    if (!io) {
        printf("%s%s", ((readback >> 1) & 0x3) == 0x2 ? "  64 bit" : "  32 bit",
               (readback & 0x8) != 0 ? "  prefetchable" : "  not prefetchable");
    }
    printf("\n");
}

static const char *tf(bool b) { return b ? "true" : "false"; }

int main(void) {
    /* decode what a BAR answers after the sizing write of all ones */
    uint32_t rb = 0xFFF00004;                   /* read back, typical */
    bool is_io   = (rb & 0x1) != 0;             /* bit 0: I/O or memory */
    bool is_64   = ((rb >> 1) & 0x3) == 0x2;    /* bits 2:1: 64 bit BAR */
    bool is_pref = (rb & 0x8) != 0;             /* bit 3: prefetchable */
    uint32_t mask = rb & ~0xFu;                 /* keep the address bits */
    uint32_t size = ~mask + 1;                  /* 0x00100000 = 1 MiB */
    /* cost: a few configuration accesses per BAR, once, at boot */

    printf("the BAR of the slide: I/O %s, 64 bit %s, prefetchable %s, size %" PRIu32 " bytes (%" PRIu32 " MiB)\n\n",
           tf(is_io), tf(is_64), tf(is_pref), size, size / (1024 * 1024));

    /* the device side of the trick: three toy BARs answer the sizing write */
    ToyBar nvme = {16 * 1024, 0x4, 0};              /* 16 KiB, memory, 64 bit */
    ToyBar gpu = {256u * 1024 * 1024, 0xC, 0};      /* 256 MiB, memory, 64 bit, prefetchable */
    ToyBar legacy = {32, 0x1, 0};                   /* 32 ports, I/O space */
    printf("sizing: write 0xFFFFFFFF, read back, invert the address bits, add one\n");
    bar_write(&nvme, 0xFFFFFFFFu);
    decode("NVMe registers", bar_read(&nvme));
    bar_write(&gpu, 0xFFFFFFFFu);
    decode("GPU aperture", bar_read(&gpu));
    bar_write(&legacy, 0xFFFFFFFFu);
    decode("legacy I/O BAR", bar_read(&legacy));

    /* then the firmware assigns an address: only the address bits stick */
    bar_write(&nvme, 0xFC500000u);
    printf("\nfirmware writes 0xFC500000 into the NVMe BAR, it reads back 0x%" PRIx32
           " (the address plus the type bits)\n", bar_read(&nvme));
    return (size == 0x00100000u && (bar_read(&nvme) & ~0xFu) == 0xFC500000u) ? 0 : 1;
}
