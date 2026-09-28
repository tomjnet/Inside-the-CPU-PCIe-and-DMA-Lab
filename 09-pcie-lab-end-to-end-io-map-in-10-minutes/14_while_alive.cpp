// Build an End to End Low Latency I/O Map in 10 Minutes - slide 14: thank you
// Build: make 14_while_alive
#include <iostream>
#include <queue>
#include <string>

// the imaginary tomjnet.h of the slide
struct Topic { std::string name; };
static int episodes = 0;
static bool alive() { return episodes < 3; }                 // three episodes, then the demo ends
static Topic next_lab_topic() {
    static const char* topics[] = {"Accelerators in 5 Minutes: Why the CPU Asks for Help",
                                   "GPU, FPGA, SmartNIC, DPU or TPU? Device Classes in 9 Minutes",
                                   "GPUs as System Accelerators in 7 Minutes"};
    return Topic{topics[episodes++]};
}
static Topic viewer_request() { return Topic{"viewer request #" + std::to_string(episodes)}; }
static void subscribe() { std::cout << "  subscribed to TomJNet\n"; }

int main() {
    std::queue<Topic> ring;
    while (alive()) {
        ring.push(next_lab_topic());     // GPU and Accelerator Lab
        ring.push(viewer_request());     // no backpressure here
        subscribe();                     // lifetime benefit
    }

    std::cout << "next lab, in ring order (" << ring.size() << " entries, none dropped):\n";
    while (!ring.empty()) {
        std::cout << "  " << ring.front().name << "\n";
        ring.pop();
    }
    return 0;
}
