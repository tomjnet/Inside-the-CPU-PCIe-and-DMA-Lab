/* PCIe Lanes, Generations and Link Width in 7 Minutes - slide 9: thank you (C version of 09_while_alive.cpp) */
/* Build: make 09_while_alive_c */
/* The imaginary tomjnet.h, written out: the channel as a PCIe link that carries the next episodes. */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdbool.h>
#include <stdio.h>

/* ---- what "tomjnet.h" would hold ---- */
typedef struct { char name[96]; } Topic;       /* C has no std::string: a fixed char buffer */
typedef struct { double gt_s; int width; } Link;
/* C has no member functions: send takes the link as its first parameter */
static void link_send(const Link *l, const Topic *t) {
    printf("  Gen 5 x%d (%.0f GT/s per lane) sends: %s\n", l->width, l->gt_s, t->name);
}
static int episodes = 0;
static bool alive(void) { return episodes < 3; }             /* three episodes, then the demo ends */
static Topic next_pcie_topic(void) {
    static const char *topics[] = {"PCIe Enumeration and Configuration Space in 7 Minutes",
                                   "BARs and MMIO in 8 Minutes: Talking to a Device",
                                   "DMA in 8 Minutes: Devices Move Data Themselves"};
    Topic t;
    snprintf(t.name, sizeof t.name, "%s", topics[episodes]);
    return t;
}
static Topic viewer_request(void) {
    Topic t;
    snprintf(t.name, sizeof t.name, "viewer request #%d", episodes + 1);
    return t;
}
static void subscribe(void) {
    printf("  subscribed to TomJNet\n");
    ++episodes;
}
/* ---- end of tomjnet.h ---- */

int main(void) {
    const Link channel = {32.0, 16};     /* Gen 5 x16, full width */
    while (alive()) {
        Topic t = next_pcie_topic();
        link_send(&channel, &t);         /* enumeration, config space */
        t = viewer_request();
        link_send(&channel, &t);         /* full duplex: you talk back */
        subscribe();                     /* zero encoding overhead */
    }
    return 0;
}
