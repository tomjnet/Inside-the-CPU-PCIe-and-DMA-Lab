/* NUMA Aware PCIe in 9 Minutes: NICs, NVMe and GPUs - slide 9: count the crossings: a placement model (C version of 09_placement_check.cpp) */
/* Build: make 09_placement_check_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdio.h>

/* one I/O path: the NUMA node of each of its four pieces */
typedef struct { int device, memory, irq, thread; } Placement;

/* every mismatch is one inter socket crossing, paid per packet */
static int crossings(const Placement *p) {
    return (p->memory != p->device ? 1 : 0)   /* remote DMA */
         + (p->irq    != p->device ? 1 : 0)   /* handler far from the ring */
         + (p->thread != p->memory ? 1 : 0);  /* worker reads far memory */
}

static void row(const char *name, Placement p) {
    const int typical_ns_per_crossing = 100;  /* an order of magnitude for the model, not a measurement */
    const int n = crossings(&p);
    printf("  %-38s%6d%8d%6d%8d%11d%9d ns\n", name, p.device, p.memory, p.irq, p.thread,
           n, n * typical_ns_per_crossing);
}

int main(void) {
    const Placement good = {1, 1, 1, 1};      /* 0 crossings */
    const Placement bad  = {1, 0, 0, 0};      /* 2 crossings: no plan */

    printf("model: NUMA node of each piece of one I/O path, the NIC sits on node 1\n");
    printf("  %-38sdevice  memory   irq  thread  crossings  model cost\n", "placement");
    row("good: everything on node 1", good);
    row("bad: nobody decided", bad);
    row("threads pinned, buffers left behind", (Placement){1, 0, 1, 1});
    row("only the IRQ on the wrong socket", (Placement){1, 1, 0, 1});
    row("memory bound, thread wandering", (Placement){1, 1, 1, 0});
    row("everything wrong", (Placement){1, 0, 0, 1});

    printf("model cost = crossings x a typical 100 ns per crossing, per packet: a model, not a\n"
           "measurement. Measure your own p99 on real hardware with two nodes.\n");

    const int ok = crossings(&good) == 0 && crossings(&bad) == 2;
    printf(ok ? "self check: ok\n" : "self check: FAILED\n");
    return ok ? 0 : 1;
}
