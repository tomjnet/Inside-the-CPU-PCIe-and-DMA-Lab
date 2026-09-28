/* Build an End to End Low Latency I/O Map in 10 Minutes - slide 14: thank you (C version of 14_while_alive.cpp) */
/* Build: make 14_while_alive_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdio.h>

/* the imaginary tomjnet.h of the slide */
typedef struct { char name[80]; } Topic;
static int episodes = 0;
static int alive(void) { return episodes < 3; }               /* three episodes, then the demo ends */
static Topic next_lab_topic(void) {
    static const char *topics[] = {"Accelerators in 5 Minutes: Why the CPU Asks for Help",
                                   "GPU, FPGA, SmartNIC, DPU or TPU? Device Classes in 9 Minutes",
                                   "GPUs as System Accelerators in 7 Minutes"};
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

/* C has no std::queue: a fixed array with a head and a tail index */
#define RING_SLOTS 8
typedef struct { Topic slot[RING_SLOTS]; int head, tail; } Ring;
static void push(Ring *r, Topic t) { if (r->tail < RING_SLOTS) r->slot[r->tail++] = t; }

int main(void) {
    static Ring ring;
    while (alive()) {
        push(&ring, next_lab_topic());   /* GPU and Accelerator Lab */
        push(&ring, viewer_request());   /* no backpressure here */
        subscribe();                     /* lifetime benefit */
    }

    printf("next lab, in ring order (%d entries, none dropped):\n", ring.tail - ring.head);
    while (ring.head != ring.tail) {
        printf("  %s\n", ring.slot[ring.head].name);
        ++ring.head;
    }
    return 0;
}
