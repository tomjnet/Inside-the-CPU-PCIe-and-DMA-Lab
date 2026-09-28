// DMA in 8 Minutes: Devices Move Data Themselves - slide 11: thank you
// Build: make 11_while_alive
#include <deque>
#include <iostream>
#include <string>

// #include "tomjnet.h"   the imaginary header of the slide: everything it would declare is below
struct Topic { std::string name; };
static int episodes = 0;
static bool alive() { return episodes < 3; }                 // three episodes, then the demo ends
static Topic next_pcie_topic() {
    static const char* topics[] = {"IOMMU: translation and isolation for devices", "MSI, MSI-X and device queues",
                                   "NUMA aware PCIe: NICs, NVMe and GPUs"};
    return Topic{topics[episodes++]};
}
static Topic viewer_request() { return Topic{"viewer request #" + std::to_string(episodes)}; }
static void subscribe() { std::cout << "  subscribed to TomJNet\n"; }

int main() {
    std::deque<Topic> ring;
    while (alive()) {
        ring.push_back(next_pcie_topic());   // IOMMU: who may DMA where
        ring.push_back(viewer_request());    // post one more buffer
        subscribe();                         // ring the doorbell
    }

    std::cout << "posted " << ring.size() << " topics:\n";
    for (const auto& t : ring) std::cout << "  " << t.name << "\n";
    return 0;
}
