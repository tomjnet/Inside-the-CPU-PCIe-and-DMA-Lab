// NUMA Aware PCIe in 9 Minutes: NICs, NVMe and GPUs - slide 9: count the crossings: a placement model
// Build: make 09_placement_check
#include <iomanip>
#include <iostream>

// one I/O path: the NUMA node of each of its four pieces
struct Placement { int device, memory, irq, thread; };

// every mismatch is one inter socket crossing, paid per packet
int crossings(const Placement& p) {
    return (p.memory != p.device ? 1 : 0)   // remote DMA
         + (p.irq    != p.device ? 1 : 0)   // handler far from the ring
         + (p.thread != p.memory ? 1 : 0);  // worker reads far memory
}

static void row(const char* name, const Placement& p) {
    const int typical_ns_per_crossing = 100;   // an order of magnitude for the model, not a measurement
    const int n = crossings(p);
    std::cout << "  " << std::left << std::setw(38) << name << std::right
              << std::setw(6) << p.device << std::setw(8) << p.memory << std::setw(6) << p.irq
              << std::setw(8) << p.thread << std::setw(11) << n
              << std::setw(9) << n * typical_ns_per_crossing << " ns\n";
}

int main() {
    Placement good{1, 1, 1, 1};                 // 0 crossings
    Placement bad {1, 0, 0, 0};                 // 2 crossings: no plan

    std::cout << "model: NUMA node of each piece of one I/O path, the NIC sits on node 1\n";
    std::cout << "  " << std::left << std::setw(38) << "placement" << std::right
              << "device  memory   irq  thread  crossings  model cost\n";
    row("good: everything on node 1", good);
    row("bad: nobody decided", bad);
    row("threads pinned, buffers left behind", Placement{1, 0, 1, 1});
    row("only the IRQ on the wrong socket", Placement{1, 1, 0, 1});
    row("memory bound, thread wandering", Placement{1, 1, 1, 0});
    row("everything wrong", Placement{1, 0, 0, 1});

    std::cout << "model cost = crossings x a typical 100 ns per crossing, per packet: a model, not a\n"
              << "measurement. Measure your own p99 on real hardware with two nodes.\n";

    const bool ok = crossings(good) == 0 && crossings(bad) == 2;
    std::cout << (ok ? "self check: ok\n" : "self check: FAILED\n");
    return ok ? 0 : 1;
}
