// MSI, MSI-X and Device Queues in 8 Minutes - slide 11: thank you
// Build: make 11_while_alive
#include <array>
#include <cstddef>
#include <iostream>
#include <queue>
#include <string>

// the imaginary tomjnet.h
struct Topic { std::string name; };
static int episodes = 0;
static bool alive() { return episodes < 3; }                 // three episodes, then the demo ends
static Topic next_pcie_topic() {
    static const char* topics[] = {"NUMA aware PCIe: NICs, NVMe and GPUs", "an end to end low latency I/O map",
                                   "the GPU and Accelerator Lab"};
    return Topic{topics[episodes++]};
}
static Topic viewer_request() { return Topic{"viewer request #" + std::to_string(episodes)}; }
static void subscribe() { std::cout << "  subscribed to TomJNet\n"; }

int main() {
    std::array<std::queue<Topic>, 4> queues;      // one per core
    while (alive()) {
        queues[0].push(next_pcie_topic());   // NUMA aware PCIe
        queues[1].push(viewer_request());    // its own vector
        subscribe();                         // no shared wire
    }

    for (std::size_t q = 0; q < queues.size(); ++q) {
        std::cout << "queue " << q << ": " << queues[q].size() << " topics\n";
        while (!queues[q].empty()) {
            std::cout << "  " << queues[q].front().name << "\n";
            queues[q].pop();
        }
    }
    return 0;
}
