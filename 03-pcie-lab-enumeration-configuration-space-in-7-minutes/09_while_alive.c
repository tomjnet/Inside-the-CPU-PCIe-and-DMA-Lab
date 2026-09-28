/* PCIe Enumeration and Configuration Space in 7 Minutes - slide 9: thank you (C version of 09_while_alive.cpp) */
/* Build: make 09_while_alive_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdbool.h>
#include <stdio.h>

/* the imaginary tomjnet.h of the slide; C has no std::string, so a Topic owns a fixed char buffer */
typedef struct { char name[80]; } Topic;
static int episodes = 0;
static bool alive(void) { return episodes < 3; }             /* three episodes, then the demo ends */
static Topic next_pcie_topic(void) {
    static const char *topics[] = {"BARs and MMIO in 8 Minutes: Talking to a Device",
                                   "DMA in 8 Minutes: Devices Move Data Themselves",
                                   "IOMMU in 8 Minutes: Translation and Isolation for Devices"};
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

/* C has no std::queue: a fixed ring with a head and a tail does the same first in, first out */
#define LAB_CAP 8
typedef struct { Topic items[LAB_CAP]; int head, tail; } Queue;
static bool push(Queue *q, Topic t) {
    if (q->tail - q->head == LAB_CAP) return false;
    q->items[q->tail++ % LAB_CAP] = t;
    return true;
}
static bool empty(const Queue *q) { return q->head == q->tail; }
static const Topic *front(const Queue *q) { return &q->items[q->head % LAB_CAP]; }
static void pop(Queue *q) { ++q->head; }

int main(void) {
    static Queue lab;                    /* zero initialised: head == tail == 0 */
    while (alive()) {
        push(&lab, next_pcie_topic());   /* next: BARs and MMIO */
        push(&lab, viewer_request());    /* one more function to scan */
        subscribe();                     /* no driver needed */
    }

    printf("queued in the lab, in order:\n");
    while (!empty(&lab)) {
        printf("  %s\n", front(&lab)->name);
        pop(&lab);
    }
    return 0;
}
