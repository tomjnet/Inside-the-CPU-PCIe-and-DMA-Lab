/* MSI, MSI-X and Device Queues in 8 Minutes - slide 5: rss: hash the flow, pick the queue (C version of 05_rss_hash.cpp) */
/* Build: make 05_rss_hash_c */
#if defined(__linux__)
#define _GNU_SOURCE
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define TABLE_SIZE 128
#define QUEUES 4
#define FLOWS 256

typedef struct { uint32_t src, dst; uint16_t sport, dport; } Flow;

/* RSS: the NIC hashes the flow in hardware. O(1), no lock, no CPU work */
static uint32_t flow_hash(const Flow *f) {   /* a model of Toeplitz */
    uint32_t h = f->src * 2654435761u;
    h ^= f->dst + (h << 6) + (h >> 2);
    h ^= (((uint32_t)f->sport << 16) | f->dport) + (h << 6);
    return h ^ (h >> 15);
}
/* indirection table (ethtool -x): the hash picks an entry, a queue */
static int queue_of(const Flow *f, const int table[TABLE_SIZE]) {
    return table[flow_hash(f) % TABLE_SIZE];   /* same flow, same queue */
}

/* fixed-seed splitmix64 replaces std::mt19937: the flows (and the example hash) differ from the C++ run */
static uint64_t rng_state = 7;
static uint32_t rng(void) {
    uint64_t z = (rng_state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return (uint32_t)((z ^ (z >> 31)) >> 32);
}

int main(void) {
    int table[TABLE_SIZE];
    for (size_t i = 0; i < TABLE_SIZE; ++i) table[i] = (int)(i % QUEUES);   /* 0 1 2 3 0 1 2 3 */

    /* 256 flows: clients on random addresses and ports, one server on port 443. Fixed seed: same work everywhere. */
    Flow flows[FLOWS];
    for (int i = 0; i < FLOWS; ++i) {
        const uint32_t src = rng();
        const uint16_t sport = (uint16_t)(1024u + rng() % 60000u);
        Flow f = { src, 0x0A000001u, sport, 443 };
        flows[i] = f;
    }

    /* property 1: every packet of one flow lands on the same queue */
    bool stable = true;
    for (int i = 0; i < FLOWS; ++i) {
        const int first = queue_of(&flows[i], table);
        for (int packet = 0; packet < 10; ++packet) stable = stable && queue_of(&flows[i], table) == first;
    }
    printf("same flow, same queue for 256 flows x 10 packets: %s\n", stable ? "yes" : "NO");

    /* property 2: many flows spread across the queues */
    int flows_on[QUEUES] = { 0 };
    for (int i = 0; i < FLOWS; ++i) ++flows_on[queue_of(&flows[i], table)];
    printf("flows per queue (256 flows, 4 queues, 64 would be perfect):\n");
    for (int q = 0; q < QUEUES; ++q) printf("  queue %d: %d\n", q, flows_on[q]);

    /* the limit: one heavy flow is still one queue, RSS balances flows, not packets */
    const Flow *heavy = &flows[0];
    printf("one heavy flow of 1000000 packets: all of them on queue %d, the other %d queues do not help\n",
           queue_of(heavy, table), QUEUES - 1);

    const Flow *f1 = &flows[1];
    printf("example: hash 0x%" PRIx32 " -> table entry %" PRIu32 " -> queue %d\n",
           flow_hash(f1), flow_hash(f1) % TABLE_SIZE, queue_of(f1, table));
    return stable ? 0 : 1;
}
