// MSI, MSI-X and Device Queues in 8 Minutes - slide 6: queue to core: three mappings
// Build: make 06_queue_to_core
#include <array>
#include <cstddef>
#include <iostream>

// one MSI-X vector per queue; its affinity names the core that runs
// the handler: /proc/irq/N/smp_affinity_list
struct Mapping { const char* name; std::array<int, 4> irq_core; };

// worker q is pinned on core q and consumes queue q
const std::array<int, 4> worker_core{0, 1, 2, 3};

const Mapping good{"one queue per core, IRQ next to its worker",
                   {0, 1, 2, 3}};
const Mapping piled{"every vector on core 0", {0, 0, 0, 0}};
const Mapping crossed{"balanced, but IRQ core is never the worker core",
                      {1, 2, 3, 0}};

static void show(const char* label, const Mapping& m) {
    std::cout << label << ": " << m.name << "\n";
    std::size_t local = 0;
    for (std::size_t q = 0; q < m.irq_core.size(); ++q) {
        const bool same = m.irq_core[q] == worker_core[q];
        if (same) ++local;
        std::cout << "  queue " << q << " -> MSI-X vector " << q << " -> IRQ on core " << m.irq_core[q]
                  << ", worker on core " << worker_core[q] << (same ? "  local\n" : "  remote: cache lines move\n");
    }
    std::cout << "  queues whose interrupt lands next to the worker: " << local << " of " << m.irq_core.size() << "\n\n";
}

int main() {
    std::cout << "The model: 4 queues, 4 MSI-X vectors, 4 cores. On a real machine the irq_core column is\n"
                 "what you read from /proc/irq/N/smp_affinity_list for the IRQ of each queue.\n\n";
    show("good", good);
    show("piled", piled);
    show("crossed", crossed);
    std::cout << "crossed is the trap: every core takes one vector, so the load looks healthy.\n";
    return 0;
}
