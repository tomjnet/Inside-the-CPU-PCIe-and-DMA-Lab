/* BARs and MMIO in 8 Minutes: Talking to a Device - slide 5: a register is not memory (C version of 05_volatile_registers.cpp) */
/* Build: make 05_volatile_registers_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

enum { CTRL = 0, STATUS = 1, NREGS = 2 };
#define READY       0x1u
#define IRQ_PENDING 0x2u

/* The "mapped BAR" of the lab is plain memory. A real driver gets this pointer from the kernel (pci_iomap). */
static uint32_t regs[NREGS] = {0, READY};   /* STATUS already says READY, so the wait loop ends */

/* What plain memory cannot show: a register whose read has a side effect. This toy device clears its
   pending bit when STATUS is read, so two loads in a row give two different answers. */
typedef struct {
    uint32_t status;
    int loads;
} ToyDevice;

static uint32_t read_status(ToyDevice *d) {
    ++d->loads;
    uint32_t seen = d->status;
    d->status = d->status & ~IRQ_PENDING;       /* read to clear */
    return seen;
}

int main(void) {
    /* without volatile the compiler may drop or merge these accesses */
    volatile uint32_t *reg = regs;              /* regs: the mapped BAR */
    reg[CTRL] = 1;                              /* store 1: start a reset */
    reg[CTRL] = 0;                              /* store 2: not a dead store */
    while ((reg[STATUS] & READY) == 0) { }      /* a fresh load every pass */
    uint32_t a = reg[STATUS];                   /* two loads, maybe two */
    uint32_t b = reg[STATUS];                   /* answers: a read can clear */
    /* every load or store is one PCIe crossing, in program order */

    printf("plain memory behind a volatile pointer: a = %" PRIu32 ", b = %" PRIu32
           " (memory never changes by itself)\n", a, b);
    printf("with volatile the binary keeps 2 stores to CTRL and 3 loads of STATUS;\n"
           "without it, -O2 may keep 1 store and 1 load. Compare: gcc -O2 -S 05_volatile_registers.c\n\n");

    ToyDevice dev = {READY | IRQ_PENDING, 0};
    uint32_t first = read_status(&dev);
    uint32_t second = read_status(&dev);
    printf("toy device with a read to clear bit: first load = %" PRIu32 ", second load = %" PRIu32
           ", loads seen by the device = %d\n", first, second, dev.loads);
    printf("a compiler that merged the two loads would have hidden the second answer\n");
    return (first != second && a == b) ? 0 : 1;
}
