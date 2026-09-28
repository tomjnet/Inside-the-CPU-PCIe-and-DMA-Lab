// BARs and MMIO in 8 Minutes: Talking to a Device - slide 11: thank you
// Build: make 11_while_alive
#include <iostream>
#include <queue>
#include <string>

// the imaginary tomjnet.h of the slide
struct Topic { std::string name; };
static int episodes = 0;
static bool alive() { return episodes < 3; }                 // three episodes, then the demo ends
static Topic next_pcie_topic() {
    static const char* topics[] = {"DMA: Devices Move Data Themselves", "IOMMU: Translation and Isolation for Devices",
                                   "MSI, MSI-X and Device Queues"};
    return Topic{topics[episodes++]};
}
static Topic viewer_request() { return Topic{"viewer request #" + std::to_string(episodes)}; }
static void subscribe() { std::cout << "  subscribed to TomJNet\n"; }

int main() {
    std::queue<Topic> ring;
    while (alive()) {
        ring.push(next_pcie_topic());       // DMA: devices move data
        ring.push(viewer_request());        // one more descriptor
        subscribe();                        // ring the doorbell
    }

    std::cout << "descriptors in the ring:\n";
    for (; !ring.empty(); ring.pop()) std::cout << "  " << ring.front().name << "\n";
    return 0;
}
