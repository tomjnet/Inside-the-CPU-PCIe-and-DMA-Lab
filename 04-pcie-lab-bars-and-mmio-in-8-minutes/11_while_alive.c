/* BARs and MMIO in 8 Minutes: Talking to a Device - slide 11: thank you (C version of 11_while_alive.cpp) */
/* Build: make 11_while_alive_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdbool.h>
#include <stdio.h>

/* the imaginary tomjnet.h of the slide */
typedef struct { char name[64]; } Topic;
static int episodes = 0;
static bool alive(void) { return episodes < 3; }             /* three episodes, then the demo ends */
static Topic next_pcie_topic(void) {
    static const char *topics[] = {"DMA: Devices Move Data Themselves", "IOMMU: Translation and Isolation for Devices",
                                   "MSI, MSI-X and Device Queues"};
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

/* C has no std::queue: a fixed array with a head and a tail index is the ring */
enum { CAP = 8 };
typedef struct { Topic items[CAP]; int head, tail; } TopicQueue;
static void push(TopicQueue *q, Topic t) { if (q->tail < CAP) q->items[q->tail++] = t; }

int main(void) {
    TopicQueue ring = {0};
    while (alive()) {
        push(&ring, next_pcie_topic());     /* DMA: devices move data */
        push(&ring, viewer_request());      /* one more descriptor */
        subscribe();                        /* ring the doorbell */
    }

    printf("descriptors in the ring:\n");
    for (; ring.head != ring.tail; ++ring.head) printf("  %s\n", ring.items[ring.head].name);
    return 0;
}
