// NUMA Aware PCIe in 9 Minutes: NICs, NVMe and GPUs - slide 12: thank you
// Build: make 12_while_alive
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

// the imaginary tomjnet.h of the slide
struct Topic { std::string name; };
static int episodes = 0;
static bool alive() { return episodes < 3; }                 // three episodes, then the demo ends
static Topic next_pcie_topic() {
    static const char* topics[] = {"Build an End to End Low Latency I/O Map",
                                   "GPU and Accelerator Lab: the next lab",
                                   "your NIC, your NVMe and your GPU on one node"};
    return Topic{topics[episodes++]};
}
static Topic viewer_request() { return Topic{"viewer request #" + std::to_string(episodes)}; }
static void subscribe() { std::cout << "  subscribed to TomJNet\n"; }

int main() {
    std::vector<Topic> local_node;
    while (alive()) {
        local_node.push_back(next_pcie_topic());  // the I/O map
        local_node.push_back(viewer_request());   // no socket crossing
        subscribe();                              // stay on this node
    }

    std::cout << "topics kept on the local node:\n";
    for (std::size_t i = 0; i < local_node.size(); ++i) {
        std::cout << "  " << i << ": " << local_node[i].name << "\n";
    }
    return 0;
}
