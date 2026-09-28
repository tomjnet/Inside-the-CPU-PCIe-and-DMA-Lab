// PCIe Lanes, Generations and Link Width in 7 Minutes - slide 4: signaling rate is not payload bandwidth
// Build: make 04_link_bandwidth
// A bandwidth calculator: generation, encoding and width in, payload GB/s per direction out.
#include <cmath>
#include <cstdio>
#include <iostream>

struct Gen { int gen; double gt_s; double payload, symbol; };
constexpr Gen gens[] = {                 // GT/s: raw rate of one lane
    {1, 2.5, 8, 10}, {2, 5.0, 8, 10},    // 8b/10b: 20% is overhead
    {3, 8.0, 128, 130}, {4, 16.0, 128, 130}, {5, 32.0, 128, 130}};

// payload GB/s, one direction: rate x encoding x lanes / 8 bits
constexpr double link_gb_s(const Gen& g, int width) {
    return g.gt_s * (g.payload / g.symbol) * width / 8.0;
}

static void print_row(int gen, double x1, double x4, double x16) {
    std::printf("  Gen %d   x1 %6.3f   x4 %6.2f   x16 %6.2f   GB/s\n", gen, x1, x4, x16);
}

int main() {
    std::cout << "the table of the slide: payload per direction\n";
    for (const Gen& g : gens)                // one row per generation
        print_row(g.gen, link_gb_s(g, 1), link_gb_s(g, 4),
                  link_gb_s(g, 16));         // Gen 4: 1.97, 7.88, 31.5

    std::cout << "\nthe full table: signaling rate, encoding, then every width in GB/s\n";
    std::printf("  %-6s %6s  %10s %9s", "gen", "GT/s", "encoding", "overhead");
    const int widths[] = {1, 2, 4, 8, 16};
    for (int w : widths) std::printf("      x%-2d", w);
    std::printf("\n");
    for (const Gen& g : gens) {
        const double overhead = 100.0 * (1.0 - g.payload / g.symbol);
        std::printf("  Gen %-2d %6.1f  %4.0fb/%3.0fb %8.1f%%", g.gen, g.gt_s, g.payload, g.symbol, overhead);
        for (int w : widths) std::printf(" %8.2f", link_gb_s(g, w));
        std::printf("\n");
    }

    std::cout << "\nwhat falls out of the table\n";
    std::printf("  Gen 3 x16 = %.2f GB/s, Gen 4 x8 = %.2f GB/s: a generation is worth a doubling of width\n",
                link_gb_s(gens[2], 16), link_gb_s(gens[3], 8));
    std::printf("  a 100 Gb NIC needs 12.50 GB/s: Gen 3 x8 gives %.2f (too little), Gen 4 x8 gives %.2f\n",
                link_gb_s(gens[2], 8), link_gb_s(gens[3], 8));
    std::printf("  NVMe on Gen 4 x4 has a ceiling of %.2f GB/s, before packet headers\n", link_gb_s(gens[3], 4));

    // correctness check, not a timing: Gen 2 x1 is exactly 0.5 GB/s and Gen 4 x16 is about 31.5
    const bool ok = std::fabs(link_gb_s(gens[1], 1) - 0.5) < 1e-12 && std::fabs(link_gb_s(gens[3], 16) - 31.5) < 0.1;
    std::cout << (ok ? "check: ok\n" : "check: FAILED\n");
    return ok ? 0 : 1;
}
