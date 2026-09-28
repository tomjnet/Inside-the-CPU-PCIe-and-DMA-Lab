// PCIe Enumeration and Configuration Space in 7 Minutes - slide 9: thank you
// Build: make 09_while_alive
#include <iostream>
#include <queue>
#include <string>

// The imaginary tomjnet.h of the slide
struct Topic { std::string name; };
static int episodes = 0;
static bool alive() { return episodes < 3; }                 // three episodes, then the demo ends
static Topic next_pcie_topic() {
    static const char* topics[] = {"BARs and MMIO in 8 Minutes: Talking to a Device",
                                   "DMA in 8 Minutes: Devices Move Data Themselves",
                                   "IOMMU in 8 Minutes: Translation and Isolation for Devices"};
    return Topic{topics[episodes++]};
}
static Topic viewer_request() { return Topic{"viewer request #" + std::to_string(episodes)}; }
static void subscribe() { std::cout << "  subscribed to TomJNet\n"; }

int main() {
    std::queue<Topic> lab;
    while (alive()) {
        lab.push(next_pcie_topic());     // next: BARs and MMIO
        lab.push(viewer_request());      // one more function to scan
        subscribe();                     // no driver needed
    }

    std::cout << "queued in the lab, in order:\n";
    while (!lab.empty()) {
        std::cout << "  " << lab.front().name << "\n";
        lab.pop();
    }
    return 0;
}
