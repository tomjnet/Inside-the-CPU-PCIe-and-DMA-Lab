/* PCIe Lanes, Generations and Link Width in 7 Minutes - slide 4: signaling rate is not payload bandwidth (C version of 04_link_bandwidth.cpp) */
/* Build: make 04_link_bandwidth_c */
/* A bandwidth calculator: generation, encoding and width in, payload GB/s per direction out. */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct { int gen; double gt_s; double payload, symbol; } Gen;
static const Gen gens[] = {              /* GT/s: raw rate of one lane */
    {1, 2.5, 8, 10}, {2, 5.0, 8, 10},    /* 8b/10b: 20% is overhead */
    {3, 8.0, 128, 130}, {4, 16.0, 128, 130}, {5, 32.0, 128, 130}};
#define GEN_COUNT (sizeof gens / sizeof gens[0])

/* payload GB/s, one direction: rate x encoding x lanes / 8 bits */
static double link_gb_s(const Gen *g, int width) {
    return g->gt_s * (g->payload / g->symbol) * width / 8.0;
}

static void print_row(int gen, double x1, double x4, double x16) {
    printf("  Gen %d   x1 %6.3f   x4 %6.2f   x16 %6.2f   GB/s\n", gen, x1, x4, x16);
}

int main(void) {
    printf("the table of the slide: payload per direction\n");
    for (size_t i = 0; i < GEN_COUNT; ++i)   /* one row per generation */
        print_row(gens[i].gen, link_gb_s(&gens[i], 1), link_gb_s(&gens[i], 4),
                  link_gb_s(&gens[i], 16));  /* Gen 4: 1.97, 7.88, 31.5 */

    printf("\nthe full table: signaling rate, encoding, then every width in GB/s\n");
    printf("  %-6s %6s  %10s %9s", "gen", "GT/s", "encoding", "overhead");
    static const int widths[] = {1, 2, 4, 8, 16};
    const size_t nw = sizeof widths / sizeof widths[0];
    for (size_t k = 0; k < nw; ++k) printf("      x%-2d", widths[k]);
    printf("\n");
    for (size_t i = 0; i < GEN_COUNT; ++i) {
        const Gen *g = &gens[i];
        const double overhead = 100.0 * (1.0 - g->payload / g->symbol);
        printf("  Gen %-2d %6.1f  %4.0fb/%3.0fb %8.1f%%", g->gen, g->gt_s, g->payload, g->symbol, overhead);
        for (size_t k = 0; k < nw; ++k) printf(" %8.2f", link_gb_s(g, widths[k]));
        printf("\n");
    }

    printf("\nwhat falls out of the table\n");
    printf("  Gen 3 x16 = %.2f GB/s, Gen 4 x8 = %.2f GB/s: a generation is worth a doubling of width\n",
           link_gb_s(&gens[2], 16), link_gb_s(&gens[3], 8));
    printf("  a 100 Gb NIC needs 12.50 GB/s: Gen 3 x8 gives %.2f (too little), Gen 4 x8 gives %.2f\n",
           link_gb_s(&gens[2], 8), link_gb_s(&gens[3], 8));
    printf("  NVMe on Gen 4 x4 has a ceiling of %.2f GB/s, before packet headers\n", link_gb_s(&gens[3], 4));

    /* correctness check, not a timing: Gen 2 x1 is exactly 0.5 GB/s and Gen 4 x16 is about 31.5 */
    const bool ok = fabs(link_gb_s(&gens[1], 1) - 0.5) < 1e-12 && fabs(link_gb_s(&gens[3], 16) - 31.5) < 0.1;
    fputs(ok ? "check: ok\n" : "check: FAILED\n", stdout);
    return ok ? 0 : 1;
}
