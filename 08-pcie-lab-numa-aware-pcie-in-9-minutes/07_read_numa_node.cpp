// NUMA Aware PCIe in 9 Minutes: NICs, NVMe and GPUs - slide 7: read numa_node for every device
// Build: make 07_read_numa_node
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

// first line of a small sysfs text file, or "?" when it is missing or unreadable
static std::string read_line(const fs::path& p) {
    std::ifstream f(p);
    std::string line;
    if (!f || !std::getline(f, line) || line.empty()) return "?";
    return line;
}

int main() {
    int seen = 0;
    std::map<int, int> per_node;                 // node number to device count, for the summary

    // one folder per device, two small text files: no PCIe crossing at all
    const fs::path root = "/sys/bus/pci/devices";
    std::error_code ec;
    for (const auto& dev : fs::directory_iterator(root, ec)) {
        int node = -1;                        // -1: firmware gave no node
        std::ifstream(dev.path() / "numa_node") >> node;
        std::string cpus = read_line(dev.path() / "local_cpulist");
        std::cout << dev.path().filename().string()
                  << "  node " << node << "  cpus " << cpus << "\n";
        ++seen;
        ++per_node[node];
    }
    if (seen == 0) std::cout << "no PCIe devices visible here (WSL)\n";

    if (ec) {
        // Windows, macOS, or a Linux without sysfs: the folder itself is missing
        std::cout << "this sample needs Linux: it would list every PCIe device with its NUMA node\n"
                  << "  (" << root.string() << ": " << ec.message() << ")\n";
    }

    if (seen > 0) {
        std::cout << "\ndevices per NUMA node:\n";
        for (const auto& [node, count] : per_node) {
            std::cout << "  node " << node << ": " << count
                      << (node < 0 ? "  (the firmware reported no node)" : "") << "\n";
        }
        if (per_node.size() <= 1) {
            std::cout << "one node only: every path is local on this machine\n";
        } else {
            std::cout << "several nodes: put buffers, IRQs and threads on the node of each device\n";
        }
    }
    std::cout << "on a two socket server the list splits in two, for example:\n"
              << "  0000:5e:00.0  node 0  cpus 0-15,32-47     (typical, not from this machine)\n"
              << "  0000:af:00.0  node 1  cpus 16-31,48-63    (typical, not from this machine)\n";
    return 0;
}
