// Build an End to End Low Latency I/O Map in 10 Minutes - slide 8: from facts to findings
// Build: make 08_findings
//
// A model: the audit of the slide runs on two built in maps, a healthy NIC and an NVMe drive with every
// fact wrong, so each finding fires once. Fill an IoMap from your own machine with 07_io_map and the
// audit scripts, then call audit() on it. Deterministic output, no Linux calls.
#include <cstddef>
#include <iostream>
#include <set>
#include <string>
#include <vector>

struct IoMap {                          // one device, one row
    std::string bdf, speed_max, speed_now;  // "16.0 GT/s PCIe"
    int width_max = 0, width_now = 0;   // x8 capable, x8 trained?
    int numa_node = -1;                 // -1: not reported
    std::string iommu = "none";         // DMA, identity or none
    int queues = 0, ring = 0;           // vectors, descriptors
    std::vector<int> irq_cpus;          // CPU of each vector
};

using CpuSet = std::set<int>;

static int findings = 0;
static void finding(const char* layer, const char* what) {
    ++findings;
    std::cout << "    [" << layer << "] " << what << "\n";
}

// "0-15,32-47" -> {0..15, 32..47}: the format of local_cpulist and smp_affinity_list
static CpuSet parse_cpulist(const std::string& list) {
    CpuSet cpus;
    std::size_t i = 0;
    while (i < list.size()) {
        int first = 0, last = 0;
        bool range = false, digits = false;
        for (; i < list.size() && list[i] != ','; ++i) {
            if (list[i] == '-') { range = true; continue; }
            if (list[i] < '0' || list[i] > '9') continue;
            digits = true;
            if (range) last = last * 10 + (list[i] - '0');
            else first = first * 10 + (list[i] - '0');
        }
        ++i;                                        // skip the comma
        if (!digits) continue;
        if (!range) last = first;
        for (int cpu = first; cpu <= last; ++cpu) cpus.insert(cpu);
    }
    return cpus;
}

// one finding per layer, in the order you fix them
void audit(const IoMap& m, int app_node, const CpuSet& local) {
    if (m.width_now < m.width_max || m.speed_now != m.speed_max)
        finding("link", "down trained: check slot, riser, BIOS");
    if (m.numa_node >= 0 && m.numa_node != app_node)
        finding("placement", "remote DMA: move threads or card");
    for (int cpu : m.irq_cpus)               // one vector per queue
        if (!local.count(cpu)) finding("irq", "vector on remote CPU");
    if (m.queues > static_cast<int>(local.size()))
        finding("queues", "more queues than local cores");
    if (m.iommu == "DMA") finding("iommu", "translated: map cost");
}

static int run(const char* title, const IoMap& m, int app_node, const std::string& local_cpulist) {
    const CpuSet local = parse_cpulist(local_cpulist);
    std::cout << title << "\n  " << m.bdf << "  x" << m.width_now << " of x" << m.width_max << ", node " << m.numa_node
              << ", iommu " << m.iommu << ", " << m.queues << " queues, ring " << m.ring << "\n";
    std::cout << "  app on node " << app_node << ", local CPUs " << local_cpulist << " (" << local.size() << " cores)\n";
    const int before = findings;
    audit(m, app_node, local);
    const int fired = findings - before;
    if (fired == 0) std::cout << "    no findings: the facts are right, now measure\n";
    return fired;
}

int main() {
    IoMap nic;
    nic.bdf = "0000:3b:00.0";
    nic.speed_max = nic.speed_now = "16.0 GT/s PCIe";
    nic.width_max = nic.width_now = 8;
    nic.numa_node = 0;
    nic.iommu = "identity";
    nic.queues = 4;
    nic.ring = 1024;
    nic.irq_cpus = {0, 1, 2, 3};

    IoMap nvme;                          // every layer wrong, on purpose
    nvme.bdf = "0000:af:00.0";
    nvme.speed_max = "16.0 GT/s PCIe";
    nvme.speed_now = "8.0 GT/s PCIe";
    nvme.width_max = 4;
    nvme.width_now = 2;
    nvme.numa_node = 1;
    nvme.iommu = "DMA";
    nvme.queues = 6;
    nvme.ring = 1024;
    nvme.irq_cpus = {0, 1, 2, 3, 16, 17};

    const int healthy = run("healthy NIC", nic, 0, "0-15,32-47");
    const int broken = run("broken NVMe drive (app pinned to four cores of node 0)", nvme, 0, "0-3");

    std::cout << "findings: healthy " << healthy << ", broken " << broken << "\n";
    std::cout << "fix order: link, placement, irq, queues, then decide about the IOMMU\n";
    // correctness: the healthy map is clean; the broken one fires link, placement, 2 irq, queues and iommu
    if (healthy != 0 || broken != 6) {
        std::cout << "unexpected number of findings\n";
        return 1;
    }
    return 0;
}
