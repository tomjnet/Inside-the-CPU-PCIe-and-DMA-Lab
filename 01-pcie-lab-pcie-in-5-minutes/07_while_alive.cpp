// PCIe in 5 Minutes: How Devices Reach the CPU - slide 7: thank you
// Build: make 07_while_alive
#include <iostream>
#include <string>
#include <vector>

// the imaginary tomjnet.h of the slide
struct Topic { std::string name; };
static int episodes = 0;
static bool alive() { return episodes < 3; }                 // three episodes, then the demo ends
static Topic next_pcie_topic() {
    static const char* topics[] = {"PCIe Lanes, Generations and Link Width", "PCIe Enumeration and Configuration Space",
                                   "BARs and MMIO: Talking to a Device"};
    return Topic{topics[episodes++]};
}
static Topic viewer_request() { return Topic{"viewer request #" + std::to_string(episodes)}; }
static void subscribe() { std::cout << "  subscribed to TomJNet\n"; }

int main() {
    std::vector<Topic> endpoints;
    while (alive()) {
        endpoints.push_back(next_pcie_topic());  // lanes and link width
        endpoints.push_back(viewer_request());   // one more endpoint
        subscribe();                             // point to point
    }

    std::cout << "endpoints attached:\n";
    for (const Topic& t : endpoints) std::cout << "  " << t.name << "\n";
    return 0;
}
