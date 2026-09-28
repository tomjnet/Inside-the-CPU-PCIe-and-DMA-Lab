// IOMMU in 8 Minutes: Translation and Isolation for Devices - slide 6: the cost: iotlb, map and unmap
// Build: make 06_iotlb_cost
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>

struct Iotlb {                           // the cache inside the IOMMU
    std::array<std::uint64_t, 8> tlb{};  // 8 entries, 0 means empty
    std::size_t next = 0, dmas = 0, walks = 0;
    void access(std::uint64_t iova) {
        const std::uint64_t p = (iova >> 12) + 1;
        ++dmas;
        if (std::find(tlb.begin(), tlb.end(), p) != tlb.end()) return;
        ++walks;                         // miss: page walk in RAM
        tlb[next++ % tlb.size()] = p;    // replace round robin
    }
    void unmap() { tlb.fill(0); }        // invalidation: all cold again
};

// A ring of `buffers` pages, one DMA per buffer, `passes` times around the ring.
// unmap_each_pass plays a driver that unmaps its buffers after every pass instead of recycling them.
static Iotlb play(std::uint64_t buffers, std::size_t passes, bool unmap_each_pass) {
    Iotlb t;
    for (std::size_t pass = 0; pass < passes; ++pass) {
        for (std::uint64_t b = 0; b < buffers; ++b) t.access(0x100000 + b * 4096);
        if (unmap_each_pass) t.unmap();
    }
    return t;
}

static void report(const char* what, const Iotlb& t) {
    const double walk_ns = 250.0;        // a typical page walk of a few memory reads, not a measurement
    std::cout << "  " << what << ": " << t.dmas << " DMAs, " << t.walks << " page walks, about "
              << static_cast<double>(t.walks) * walk_ns / 1000.0 << " microseconds of walks (typical cost)\n";
}

int main() {
    const std::size_t passes = 1000;
    const Iotlb hot = play(8, passes, false);        // 8 buffers kept mapped: they fit in the IOTLB
    const Iotlb wide = play(16, passes, false);      // 16 buffers, 8 entries: the ring evicts itself
    const Iotlb churn = play(8, passes, true);       // 8 buffers, unmapped and mapped again every pass

    std::cout << "IOTLB model: 8 entries, 4 KiB pages, " << passes << " passes around the ring\n";
    report("8 buffers, mapped once and recycled ", hot);
    report("16 buffers, mapped once and recycled", wide);
    report("8 buffers, unmap after every pass   ", churn);
    std::cout << "the unmap itself also costs CPU time: a command to the IOMMU and a wait, per invalidation\n";
    std::cout << "a real IOTLB is larger, and a 2 MiB page covers 512 times more per entry: same rule, other numbers\n";

    const bool ok = hot.walks == 8 && wide.walks == 16 * passes && churn.walks == 8 * passes;
    std::cout << (ok ? "model check: OK\n" : "model check: FAILED\n");
    return ok ? 0 : 1;
}
