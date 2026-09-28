// PCIe Lanes, Generations and Link Width in 7 Minutes - slide 9: thank you
// Build: make 09_while_alive
// The imaginary tomjnet.h, written out: the channel as a PCIe link that carries the next episodes.
#include <cstdio>
#include <string>

// ---- what "tomjnet.h" would hold ----
struct Topic { std::string name; };
struct Link {
    double gt_s;
    int width;
    void send(const Topic& t) const {
        std::printf("  Gen 5 x%d (%.0f GT/s per lane) sends: %s\n", width, gt_s, t.name.c_str());
    }
};
static int episodes = 0;
static bool alive() { return episodes < 3; }                 // three episodes, then the demo ends
static Topic next_pcie_topic() {
    static const char* topics[] = {"PCIe Enumeration and Configuration Space in 7 Minutes",
                                   "BARs and MMIO in 8 Minutes: Talking to a Device",
                                   "DMA in 8 Minutes: Devices Move Data Themselves"};
    return Topic{topics[episodes]};
}
static Topic viewer_request() { return Topic{"viewer request #" + std::to_string(episodes + 1)}; }
static void subscribe() {
    std::printf("  subscribed to TomJNet\n");
    ++episodes;
}
// ---- end of tomjnet.h ----

int main() {
    Link channel{32.0, 16};              // Gen 5 x16, full width
    while (alive()) {
        channel.send(next_pcie_topic()); // enumeration, config space
        channel.send(viewer_request());  // full duplex: you talk back
        subscribe();                     // zero encoding overhead
    }
    return 0;
}
