/* MSI, MSI-X and Device Queues in 8 Minutes - slide 7: per core load: good against bad (C version of 07_core_load.cpp) */
/* Build: make 07_core_load_c */
#if defined(__linux__)
#define _GNU_SOURCE
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define TABLE_SIZE 128
#define CORES 4
#define FLOWS 4096
#define PACKETS 100000

typedef struct { uint32_t src, dst; uint16_t sport, dport; } Flow;

static uint32_t flow_hash(const Flow *f) {   /* the model of slide 5 */
    uint32_t h = f->src * 2654435761u;
    h ^= f->dst + (h << 6) + (h >> 2);
    h ^= (((uint32_t)f->sport << 16) | f->dport) + (h << 6);
    return h ^ (h >> 15);
}
static int queue_of(const Flow *f, const int table[TABLE_SIZE]) {
    return table[flow_hash(f) % TABLE_SIZE];
}

typedef struct { const char *name; int irq_core[CORES]; } Mapping;   /* slide 6 */
static const int worker_core[CORES] = { 0, 1, 2, 3 };               /* worker q pinned on core q */

typedef struct { long load[CORES]; long remote; } Result;

static Result run(const Mapping *m, const Flow *packets, size_t n, const int table[TABLE_SIZE]) {
    Result r = { { 0 }, 0 };
    /* per core load: where does the interrupt of each packet land? */
    for (size_t i = 0; i < n; ++i) {
        int q = queue_of(&packets[i], table);   /* RSS picked the queue */
        int core = m->irq_core[q];              /* MSI-X vector affinity */
        ++r.load[core];                         /* handler runs here */
        if (core != worker_core[q])             /* data sits in another cache: */
            ++r.remote;                         /* the worker starts with misses */
    }
    return r;
}

/* fixed-seed splitmix64 replaces std::mt19937 */
static uint64_t rng_state = 7;
static uint32_t rng(void) {
    uint64_t z = (rng_state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return (uint32_t)((z ^ (z >> 31)) >> 32);
}

int main(void) {
    int table[TABLE_SIZE];
    for (size_t i = 0; i < TABLE_SIZE; ++i) table[i] = (int)(i % CORES);

    /* 100000 packets from 4096 flows, fixed seed so every run and every toolchain does the same work */
    Flow *flows = malloc(FLOWS * sizeof *flows);
    Flow *packets = malloc(PACKETS * sizeof *packets);
    if (flows == NULL || packets == NULL) {
        printf("out of memory\n");
        free(flows);
        free(packets);
        return 1;
    }
    for (int i = 0; i < FLOWS; ++i) {
        const uint32_t src = rng();
        const uint16_t sport = (uint16_t)(1024u + rng() % 60000u);
        Flow f = { src, 0x0A000001u, sport, 443 };
        flows[i] = f;
    }
    for (int i = 0; i < PACKETS; ++i) packets[i] = flows[rng() % FLOWS];

    static const Mapping mappings[3] = {
        { "good: IRQ of queue q on core q", { 0, 1, 2, 3 } },
        { "piled: every vector on core 0", { 0, 0, 0, 0 } },
        { "crossed: balanced, IRQ core is never the worker core", { 1, 2, 3, 0 } },
    };

    const long total = PACKETS;
    bool ok = true;
    long remotes[3] = { 0 };
    for (int i = 0; i < 3; ++i) {
        const Result r = run(&mappings[i], packets, PACKETS, table);
        printf("%s\n", mappings[i].name);
        long sum = 0;
        for (int c = 0; c < CORES; ++c) {
            const double pct = 100.0 * (double)r.load[c] / (double)total;
            char bar[48];
            int len = (int)(pct / 2.5);
            if (len > (int)sizeof bar - 1) len = (int)sizeof bar - 1;
            for (int k = 0; k < len; ++k) bar[k] = '#';
            bar[len] = '\0';
            printf("  core %d: %6ld interrupts  %5.1f%%  %s\n", c, r.load[c], pct, bar);
            sum += r.load[c];
        }
        printf("  remote packets (IRQ core is not the worker core): %ld of %ld = %.1f%%\n\n", r.remote, total,
               100.0 * (double)r.remote / (double)total);
        ok = ok && sum == total;                /* the model loses no packet */
        remotes[i] = r.remote;
    }
    ok = ok && remotes[0] == 0 && remotes[2] == total;   /* good is all local, crossed is all remote */

    printf("Typical cost of a remote packet: its descriptor and headers sit in the cache of the IRQ core,\n"
           "so the worker pays tens of nanoseconds per cache line instead of about one. Even load is not\n"
           "locality: crossed looks as balanced as good and is remote for every packet.\n");
    free(packets);   /* C has no destructor: free what malloc gave */
    free(flows);
    return ok ? 0 : 1;
}
