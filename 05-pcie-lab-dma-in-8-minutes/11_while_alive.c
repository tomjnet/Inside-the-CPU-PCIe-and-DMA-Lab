/* DMA in 8 Minutes: Devices Move Data Themselves - slide 11: thank you (C version of 11_while_alive.cpp) */
/* Build: make 11_while_alive_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stddef.h>
#include <stdio.h>

/* #include "tomjnet.h"   the imaginary header of the slide: everything it would declare is below */
typedef struct { char name[64]; } Topic;
static int episodes = 0;
static int alive(void) { return episodes < 3; }             /* three episodes, then the demo ends */
static Topic next_pcie_topic(void) {
    static const char *topics[] = { "IOMMU: translation and isolation for devices", "MSI, MSI-X and device queues",
                                    "NUMA aware PCIe: NICs, NVMe and GPUs" };
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
    /* C has no std::deque: a fixed ring with a count, sized for the three episodes */
    Topic ring[8];
    size_t count = 0;
    while (alive()) {
        ring[count++] = next_pcie_topic();   /* IOMMU: who may DMA where */
        ring[count++] = viewer_request();    /* post one more buffer */
        subscribe();                         /* ring the doorbell */
    }

    printf("posted %zu topics:\n", count);
    for (size_t i = 0; i < count; ++i) printf("  %s\n", ring[i].name);
    return 0;
}
