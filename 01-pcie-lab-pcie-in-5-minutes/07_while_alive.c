/* PCIe in 5 Minutes: How Devices Reach the CPU - slide 7: thank you (C version of 07_while_alive.cpp) */
/* Build: make 07_while_alive_c */
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
    static const char *topics[] = {"PCIe Lanes, Generations and Link Width", "PCIe Enumeration and Configuration Space",
                                   "BARs and MMIO: Talking to a Device"};
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
    /* C has no std::vector: three episodes of two topics fit a fixed array */
    Topic endpoints[6];
    int count = 0;
    while (alive() && count + 2 <= 6) {
        endpoints[count++] = next_pcie_topic();   /* lanes and link width */
        endpoints[count++] = viewer_request();    /* one more endpoint */
        subscribe();                              /* point to point */
    }

    printf("endpoints attached:\n");
    for (int i = 0; i < count; ++i) printf("  %s\n", endpoints[i].name);
    return 0;
}
