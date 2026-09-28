/* BARs and MMIO in 8 Minutes: Talking to a Device - slide 8: ring the doorbell in the model (C version of 08_doorbell.cpp) */
/* Build: make 08_doorbell_c */
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

#define RING 16u

typedef struct {
    uint32_t ctrl;
    uint32_t status;
    uint32_t irq_mask;
    uint32_t doorbell;
} Regs;
typedef struct { uint32_t opcode; uint32_t payload; } Cmd;
typedef struct {
    Cmd ring[RING];
    uint32_t tail;                     /* owned by the driver */
} Queue;

/* driver side of a doorbell: prepare in RAM, then one MMIO write */
static void submit(Queue *q, volatile Regs *dev, const Cmd *cmds, int n) {
    for (int i = 0; i < n; ++i) {
        q->ring[q->tail] = cmds[i];         /* plain RAM: cheap, cached */
        q->tail = (q->tail + 1) % RING;     /* the device is not told */
    }
    dev->doorbell = q->tail;                /* 1 PCIe crossing, n cmds */
}
/* device side of the model: runs when the doorbell is written
   while (head != tail) { run(ring[head]); head = (head + 1) % RING; } */

typedef struct {
    uint32_t head;                     /* owned by the device */
    uint64_t executed;
    uint64_t checksum;
} ToyDevice;

static void on_doorbell(ToyDevice *d, const Queue *q, const volatile Regs *dev) {
    uint32_t tail = dev->doorbell;
    while (d->head != tail) {
        d->checksum += (uint64_t)q->ring[d->head].opcode * 1000u + q->ring[d->head].payload;
        ++d->executed;
        d->head = (d->head + 1) % RING;
    }
}

enum { N = 12 };

int main(void) {
    Cmd cmds[N];
    for (int i = 0; i < N; ++i) {
        cmds[i].opcode = (uint32_t)(i % 3 + 1);
        cmds[i].payload = (uint32_t)(i * 7);
    }

    /* one doorbell per command: every call of submit ends in exactly one doorbell store */
    static Regs bar_a;
    static Queue qa;                        /* static: zero initialized, like the C++ member defaults */
    ToyDevice da = {0, 0, 0};
    int per_command = 0;
    for (int i = 0; i < N; ++i) {
        submit(&qa, &bar_a, &cmds[i], 1);
        ++per_command;
        on_doorbell(&da, &qa, &bar_a);      /* the hardware reacts to the write */
    }

    /* one doorbell per batch */
    static Regs bar_b;
    static Queue qb;
    ToyDevice db = {0, 0, 0};
    int per_batch = 0;
    submit(&qb, &bar_b, cmds, N);
    ++per_batch;
    on_doorbell(&db, &qb, &bar_b);

    const double crossing_ns = 200.0;       /* a typical order of magnitude for one MMIO write, not a measurement */
    printf("%d commands, ring of %u slots\n", N, RING);
    printf("  ring per command: %d doorbell writes, about %g ns of link crossings (typical), device executed %" PRIu64 "\n",
           per_command, per_command * crossing_ns, da.executed);
    printf("  ring per batch:   %d doorbell write,  about %g ns of link crossings (typical), device executed %" PRIu64 "\n",
           per_batch, per_batch * crossing_ns, db.executed);
    printf("same work on the device: checksum %" PRIu64 " and %" PRIu64 "\n", da.checksum, db.checksum);
    printf("the trade: a batch waits for its last command, so a latency sensitive driver rings early when idle\n");
    bool ok = da.executed == (uint64_t)N && db.executed == da.executed && da.checksum == db.checksum;
    return ok ? 0 : 1;
}
