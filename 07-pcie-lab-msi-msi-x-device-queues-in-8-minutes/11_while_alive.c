/* MSI, MSI-X and Device Queues in 8 Minutes - slide 11: thank you (C version of 11_while_alive.cpp) */
/* Build: make 11_while_alive_c */
#if defined(__linux__)
#define _GNU_SOURCE
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdbool.h>
#include <stdio.h>

#define QUEUE_CAP 8

/* the imaginary tomjnet.h */
typedef struct { char name[64]; } Topic;
/* C has no std::queue: a fixed ring buffer with a head and a count does the same job */
typedef struct { Topic items[QUEUE_CAP]; int head; int count; } TopicQueue;

static bool push(TopicQueue *q, Topic t) {
    if (q->count == QUEUE_CAP) return false;
    q->items[(q->head + q->count) % QUEUE_CAP] = t;
    ++q->count;
    return true;
}
static Topic pop(TopicQueue *q) {
    Topic t = q->items[q->head];
    q->head = (q->head + 1) % QUEUE_CAP;
    --q->count;
    return t;
}

static int episodes = 0;
static bool alive(void) { return episodes < 3; }   /* three episodes, then the demo ends */
static Topic next_pcie_topic(void) {
    static const char *topics[] = { "NUMA aware PCIe: NICs, NVMe and GPUs", "an end to end low latency I/O map",
                                    "the GPU and Accelerator Lab" };
    Topic t;
    snprintf(t.name, sizeof t.name, "%s", topics[episodes++]);
    return t;
}
static Topic viewer_request(void) {
    Topic t;
    snprintf(t.name, sizeof t.name, "viewer request #%d", episodes);
    return t;
}
static void subscribe(void) { printf("  subscribed to TomJNet\n"); }

int main(void) {
    TopicQueue queues[4] = { 0 };   /* one per core */
    while (alive()) {
        push(&queues[0], next_pcie_topic());   /* NUMA aware PCIe */
        push(&queues[1], viewer_request());    /* its own vector */
        subscribe();                           /* no shared wire */
    }

    for (int q = 0; q < 4; ++q) {
        printf("queue %d: %d topics\n", q, queues[q].count);
        while (queues[q].count > 0) {
            Topic t = pop(&queues[q]);
            printf("  %s\n", t.name);
        }
    }
    return 0;
}
