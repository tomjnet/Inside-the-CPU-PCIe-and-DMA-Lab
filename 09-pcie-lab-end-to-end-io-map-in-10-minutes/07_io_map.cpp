// Build an End to End Low Latency I/O Map in 10 Minutes - slide 7: the map as a program
// Build: make 07_io_map
//
// Prints one I/O map row per PCIe device from sysfs: link (capable against trained), NUMA node, IOMMU mode,
// MSI or MSI-X vectors and the CPU each vector may run on. sysfs is plain text, so the program is portable
// C++: where /sys/bus/pci/devices does not exist or is empty (Windows, WSL) it says so and prints built in
// sample data instead. Ring sizes are not in sysfs: read them with ethtool -g (see 06_audit_queues_irqs_rings.sh).
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

struct IoMap {                          // one device, one row
    std::string bdf, speed_max, speed_now;  // "16.0 GT/s PCIe"
    int width_max = 0, width_now = 0;   // x8 capable, x8 trained?
    int numa_node = -1;                 // -1: not reported
    std::string iommu = "none";         // DMA, identity or none
    int queues = 0, ring = 0;           // vectors, descriptors
    std::vector<int> irq_cpus;          // CPU of each vector
};
std::string read_sysfs(const std::string& dev, const char* f) {
    std::ifstream in("/sys/bus/pci/devices/" + dev + "/" + f);
    std::string s; std::getline(in, s); return s;  // "" if absent
}                       // a text file read: zero PCIe crossings

// "8" -> 8, "" or text -> fallback (sysfs values are short decimal numbers, -1 for an unknown NUMA node)
static int to_int(const std::string& s, int fallback) {
    if (s.empty()) return fallback;
    std::size_t i = 0;
    bool negative = false;
    if (s[0] == '-') { negative = true; i = 1; }
    if (i >= s.size() || s[i] < '0' || s[i] > '9') return fallback;
    int value = 0;
    for (; i < s.size() && s[i] >= '0' && s[i] <= '9'; ++i) value = value * 10 + (s[i] - '0');
    return negative ? -value : value;
}

// the first CPU of a list such as "0-15,32-47" or "3": enough to say where a vector lands
static int first_cpu(const std::string& list) { return to_int(list, -1); }

static IoMap load(const std::string& bdf) {
    IoMap m;
    m.bdf = bdf;
    m.speed_max = read_sysfs(bdf, "max_link_speed");
    m.speed_now = read_sysfs(bdf, "current_link_speed");
    m.width_max = to_int(read_sysfs(bdf, "max_link_width"), 0);
    m.width_now = to_int(read_sysfs(bdf, "current_link_width"), 0);
    m.numa_node = to_int(read_sysfs(bdf, "numa_node"), -1);
    const std::string type = read_sysfs(bdf, "iommu_group/type");   // DMA, DMA-FQ or identity
    if (!type.empty()) m.iommu = type.rfind("DMA", 0) == 0 ? "DMA" : type;

    // one file per MSI or MSI-X vector, named by its IRQ number
    std::error_code ec;
    std::filesystem::directory_iterator it("/sys/bus/pci/devices/" + bdf + "/msi_irqs", ec);
    for (; !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
        const std::string irq = it->path().filename().string();
        std::ifstream aff("/proc/irq/" + irq + "/smp_affinity_list");
        std::string cpus;
        std::getline(aff, cpus);
        m.irq_cpus.push_back(first_cpu(cpus));
    }
    std::sort(m.irq_cpus.begin(), m.irq_cpus.end());
    m.queues = static_cast<int>(m.irq_cpus.size());
    return m;
}

static std::vector<IoMap> sample_data() {
    IoMap nic;
    nic.bdf = "0000:3b:00.0";
    nic.speed_max = nic.speed_now = "16.0 GT/s PCIe";
    nic.width_max = nic.width_now = 8;
    nic.numa_node = 0;
    nic.iommu = "identity";
    nic.queues = 4;
    nic.ring = 1024;
    nic.irq_cpus = {0, 1, 2, 3};

    IoMap nvme;                          // trained at half its width, on the other socket
    nvme.bdf = "0000:af:00.0";
    nvme.speed_max = nvme.speed_now = "16.0 GT/s PCIe";
    nvme.width_max = 4;
    nvme.width_now = 2;
    nvme.numa_node = 1;
    nvme.iommu = "DMA";
    nvme.queues = 8;
    nvme.ring = 1024;
    nvme.irq_cpus = {0, 1, 2, 3, 16, 17, 18, 19};
    return {nic, nvme};
}

static void print(const IoMap& m) {
    std::cout << "  " << m.bdf << "\n";
    std::cout << "    1 link    x" << m.width_now << " at " << (m.speed_now.empty() ? "?" : m.speed_now)
              << "   capable: x" << m.width_max << " at " << (m.speed_max.empty() ? "?" : m.speed_max);
    if (m.width_now < m.width_max || m.speed_now != m.speed_max) std::cout << "   [down trained]";
    std::cout << "\n";
    std::cout << "    2 NUMA    node " << m.numa_node << (m.numa_node < 0 ? " (not reported: single node)" : "") << "\n";
    std::cout << "    3 IOMMU   " << m.iommu << "\n";
    std::cout << "    4 queues  " << m.queues << " vectors\n";
    std::cout << "    5 IRQs    CPUs:";
    for (int cpu : m.irq_cpus) std::cout << " " << cpu;
    if (m.irq_cpus.empty()) std::cout << " none (no MSI vectors, or the driver is not bound)";
    std::cout << "\n";
    if (m.ring > 0) std::cout << "    6 ring    " << m.ring << " descriptors\n";
    else std::cout << "    6 ring    not in sysfs: ethtool -g <nic>\n";
}

int main() {
    std::vector<IoMap> maps;
    std::error_code ec;
    std::filesystem::directory_iterator it("/sys/bus/pci/devices", ec);
    for (; !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
        const std::string bdf = it->path().filename().string();
        if (read_sysfs(bdf, "max_link_width").empty()) continue;      // not a PCIe link (host bridge, legacy)
        if (to_int(read_sysfs(bdf, "max_link_width"), 0) == 0) continue;
        maps.push_back(load(bdf));
    }
    std::sort(maps.begin(), maps.end(), [](const IoMap& a, const IoMap& b) { return a.bdf < b.bdf; });

    if (maps.empty()) {
        std::cout << "no PCIe devices visible here (Windows, WSL or a container): built in sample data\n";
        maps = sample_data();
    } else {
        std::cout << "I/O map from /sys/bus/pci/devices, " << maps.size() << " PCIe devices (first 8 shown)\n";
        if (maps.size() > 8) maps.resize(8);
    }
    for (const IoMap& m : maps) print(m);
    std::cout << "six facts per device: run 08_findings to turn them into findings\n";
    return 0;
}
