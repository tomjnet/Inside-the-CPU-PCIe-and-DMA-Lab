// IOMMU in 8 Minutes: Translation and Isolation for Devices - slide 11: thank you
// Build: make 11_while_alive
#include <iostream>
#include <queue>
#include <string>

// the imaginary "tomjnet.h" of the slide
struct Topic { std::string name; };
static int episodes = 0;
static bool alive() { return episodes < 3; }                 // three episodes, then the demo ends
static Topic next_pcie_topic() {
    static const char* topics[] = {"MSI, MSI-X and Device Queues in 8 Minutes",
                                   "NUMA Aware PCIe in 9 Minutes: NICs, NVMe and GPUs",
                                   "Build an End to End Low Latency I/O Map in 10 Minutes"};
    return Topic{topics[episodes++]};
}
static Topic viewer_request() { return Topic{"viewer request #" + std::to_string(episodes)}; }
static void subscribe() { std::cout << "  subscribed to TomJNet\n"; }

int main() {
    std::queue<Topic> todo;
    while (alive()) {
        todo.push(next_pcie_topic());    // MSI-X and device queues
        todo.push(viewer_request());     // your device, your question
        subscribe();                     // lifetime benefit
    }

    std::cout << "next in the PCIe and DMA Lab:\n";
    while (!todo.empty()) {
        std::cout << "  " << todo.front().name << "\n";
        todo.pop();
    }
    return 0;
}
