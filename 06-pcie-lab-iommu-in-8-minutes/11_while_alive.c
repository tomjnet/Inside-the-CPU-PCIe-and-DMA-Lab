/* IOMMU in 8 Minutes: Translation and Isolation for Devices - slide 11: thank you (C version of 11_while_alive.cpp) */
/* Build: make 11_while_alive_c */
#if defined(__linux__)
#define _GNU_SOURCE
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdbool.h>
#include <stdio.h>

/* the imaginary "tomjnet.h" of the slide */
typedef struct { char name[64]; } Topic;
static int episodes = 0;
static bool alive(void) { return episodes < 3; }             /* three episodes, then the demo ends */
static Topic next_pcie_topic(void) {
    static const char *topics[] = {"MSI, MSI-X and Device Queues in 8 Minutes",
                                   "NUMA Aware PCIe in 9 Minutes: NICs, NVMe and GPUs",
                                   "Build an End to End Low Latency I/O Map in 10 Minutes"};
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

/* C has no std::queue: a fixed ring with a head and a tail index */
#define TODO_CAP 16
typedef struct { Topic items[TODO_CAP]; int head, tail; } Queue;
static void push(Queue *q, Topic t) { if (q->tail - q->head < TODO_CAP) q->items[q->tail++ % TODO_CAP] = t; }
static bool empty(const Queue *q) { return q->head == q->tail; }

int main(void) {
    static Queue todo;
    while (alive()) {
        push(&todo, next_pcie_topic());    /* MSI-X and device queues */
        push(&todo, viewer_request());     /* your device, your question */
        subscribe();                       /* lifetime benefit */
    }

    printf("next in the PCIe and DMA Lab:\n");
    while (!empty(&todo)) {
        printf("  %s\n", todo.items[todo.head % TODO_CAP].name);
        ++todo.head;
    }
    return 0;
}
