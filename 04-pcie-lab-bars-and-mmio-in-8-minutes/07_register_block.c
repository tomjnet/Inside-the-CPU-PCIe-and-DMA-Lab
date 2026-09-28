/* BARs and MMIO in 8 Minutes: Talking to a Device - slide 7: a register block model (C version of 07_register_block.cpp) */
/* Build: make 07_register_block_c */
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

/* the register block of a toy device, as its data sheet draws it */
typedef struct {                   /* offsets inside BAR 0 */
    uint32_t ctrl;                 /* 0x00 write: enable, reset */
    uint32_t status;               /* 0x04 read: ready, error */
    uint32_t irq_mask;             /* 0x08 */
    uint32_t doorbell;             /* 0x0C write: new queue tail */
} Regs;

/* the layout must match the data sheet byte for byte: check it at compile time */
_Static_assert(sizeof(Regs) == 16, "four 32 bit registers, no padding");
_Static_assert(offsetof(Regs, status) == 0x04, "status at offset 0x04");
_Static_assert(offsetof(Regs, irq_mask) == 0x08, "irq_mask at offset 0x08");
_Static_assert(offsetof(Regs, doorbell) == 0x0C, "doorbell at offset 0x0C");

int main(void) {
    /* a real driver asks the kernel: base = pci_iomap(pdev, 0, 0) */
    static Regs bar0;                  /* the lab: plain memory, no device */
    volatile Regs *dev = &bar0;        /* same code shape as the driver */
    dev->ctrl = 1;                     /* one store = one MMIO write */
    uint32_t s = dev->status;          /* one load  = one MMIO read */

    printf("register block: %u bytes, layout checked with _Static_assert\n"
           "  ctrl     at 0x00\n"
           "  status   at 0x04\n"
           "  irq_mask at 0x08\n"
           "  doorbell at 0x0C\n", (unsigned)sizeof(Regs));
    uint32_t ctrl_now = dev->ctrl;
    printf("after the store: ctrl = %" PRIu32 ", status read = %" PRIu32 "\n", ctrl_now, s);
    printf("this base is plain memory at %p; the base of a driver comes from the kernel and can change from boot to boot\n",
           (void *)&bar0);
    return ctrl_now == 1 ? 0 : 1;
}
