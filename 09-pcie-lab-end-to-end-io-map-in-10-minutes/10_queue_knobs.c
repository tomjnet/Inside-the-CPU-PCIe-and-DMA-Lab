/* Build an End to End Low Latency I/O Map in 10 Minutes - slide 10: a model of the knobs (C version of 10_queue_knobs.cpp) */
/* Build: make 10_queue_knobs_c */
/*
 * A model in ticks, not a measurement: a burst of 40 packets arrives every 64 ticks in a ring of `depth`
 * slots; the core wakes up, pays a fixed cost and harvests up to `batch` packets. The table shows the three
 * knobs of the slide: a ring smaller than the burst drops, a batch of one cannot keep up and a deeper ring
 * only turns its drops into waiting, a batch spreads the fixed cost. Deterministic output.
 */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stddef.h>
#include <stdio.h>

#define MAX_DEPTH 1024

typedef struct {
    long long arrived, dropped, wakeups, wait, delivered, left;
} Stats;

static Stats run(size_t depth, size_t batch) {
    Stats s = {0, 0, 0, 0, 0, 0};
    /* the ring: the arrival tick of every packet in it; C has no std::deque, so head plus count */
    static int q[MAX_DEPTH];
    size_t head = 0, count = 0;
    int busy = 0;
    if (depth > MAX_DEPTH) depth = MAX_DEPTH;

    /* a wake up costs 4 ticks fixed (IRQ, doorbell) plus 1 per packet */
    for (int t = 0; t < 64000; ++t) {
        for (int i = 0; t % 64 == 0 && i < 40; ++i) {   /* burst of 40 */
            if (count < depth) { q[(head + count) % depth] = t; ++count; }
            else ++s.dropped;                  /* full ring: backpressure */
        }
        if (busy > 0) { --busy; continue; }    /* core still working */
        size_t n = batch < count ? batch : count;       /* one batch */
        if (n > 0) { busy = 3 + (int)n; ++s.wakeups; }
        for (; n > 0; --n) { s.wait += t - q[head]; head = (head + 1) % depth; --count; }
    }                                          /* busy: this tick + 3 + n */

    s.arrived = 40LL * (64000 / 64);
    s.left = (long long)count;
    s.delivered = s.arrived - s.dropped - s.left;
    return s;
}

int main(void) {
    const size_t depths[] = {16, 64, 1024};
    const size_t batches[] = {1, 8, 32};
    int ok = 1;

    printf("bursts of 40 packets every 64 ticks, 1000 bursts; a wake up costs 4 ticks + 1 per packet\n");
    printf("  depth  batch   delivered   dropped   wake ups   mean wait (ticks)   ticks per packet\n");
    for (size_t di = 0; di < sizeof depths / sizeof depths[0]; ++di) {
        for (size_t bi = 0; bi < sizeof batches / sizeof batches[0]; ++bi) {
            const size_t depth = depths[di], batch = batches[bi];
            const Stats s = run(depth, batch);
            const double done = (double)s.delivered;
            const double mean_wait = s.delivered > 0 ? (double)s.wait / done : 0.0;
            const double cost = s.delivered > 0 ? (double)(4 * s.wakeups + s.delivered) / done : 0.0;
            printf("%7zu%7zu%12lld%10lld%11lld%20.1f%19.1f\n", depth, batch, s.delivered, s.dropped, s.wakeups,
                   mean_wait, cost);
            /* every packet is delivered, dropped or still in the ring, and the ring never exceeds its depth */
            if (s.delivered < 0 || s.left > (long long)depth) ok = 0;
        }
    }
    printf("depth 16: the burst of 40 does not fit, at least 24 of every 40 are dropped whatever the batch\n");
    printf("batch 1: 5 ticks per packet, the core cannot keep up; depth 1024 only makes each packet wait longer\n");
    printf("depth 64, batch 32: no drops, far fewer wake ups, a short wait\n");
    printf("this batch never waits to fill: a coalescing timer that does wait adds that delay to every packet\n");
    if (!ok) {
        printf("model accounting is wrong\n");
        return 1;
    }
    return 0;
}
