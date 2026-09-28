/* NUMA Aware PCIe in 9 Minutes: NICs, NVMe and GPUs - slide 12: thank you (C version of 12_while_alive.cpp) */
/* Build: make 12_while_alive_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stddef.h>
#include <stdio.h>

/* the imaginary tomjnet.h of the slide */
typedef struct { char name[64]; } Topic;
static int episodes = 0;
static int alive(void) { return episodes < 3; }              /* three episodes, then the demo ends */
static Topic next_pcie_topic(void) {
    static const char *topics[] = {"Build an End to End Low Latency I/O Map",
                                   "GPU and Accelerator Lab: the next lab",
                                   "your NIC, your NVMe and your GPU on one node"};
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
    Topic local_node[6];                          /* fixed size: three rounds of two, no std::vector */
    size_t count = 0;
    while (alive()) {
        local_node[count++] = next_pcie_topic();  /* the I/O map */
        local_node[count++] = viewer_request();   /* no socket crossing */
        subscribe();                              /* stay on this node */
    }

    printf("topics kept on the local node:\n");
    for (size_t i = 0; i < count; ++i) {
        printf("  %zu: %s\n", i, local_node[i].name);
    }
    return 0;
}
