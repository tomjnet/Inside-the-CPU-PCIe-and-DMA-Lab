// IOMMU in 8 Minutes: Translation and Isolation for Devices - slide 5: a model of the iova table
// Build: make 05_iova_table
#include <cstdint>
#include <iostream>
#include <map>
#include <optional>

struct Entry { std::uint64_t phys; bool write; };
using Domain = std::map<std::uint64_t, Entry>;   // IOVA page -> entry

// One lookup per DMA: the device never sees a physical address
std::optional<std::uint64_t> translate(const Domain& d,
                                       std::uint64_t iova, bool wr) {
    auto it = d.find(iova >> 12);                // 4 KiB pages
    if (it == d.end()) return std::nullopt;      // not mapped: fault
    if (wr && !it->second.write) return std::nullopt;   // read only
    return it->second.phys | (iova & 0xFFF);     // frame plus offset
}
// NIC domain maps IOVA 0x1000 only: a DMA to 0x9000 is blocked

// One DMA of the model: prints what the IOMMU does with it and says whether that was the expected result.
static bool dma(const char* bdf, const Domain& d, std::uint64_t iova, bool wr, bool expect_ok) {
    const auto phys = translate(d, iova, wr);
    std::cout << "  " << bdf << (wr ? " DMA write" : " DMA read ") << " IOVA 0x" << std::hex << iova;
    if (phys) {
        std::cout << " -> physical 0x" << *phys << std::dec << "\n";
    } else {
        std::cout << std::dec << " -> BLOCKED\n";
        std::cout << "    log (model): [" << (wr ? "DMA Write" : "DMA Read") << "] request device [" << bdf
                  << "] fault addr 0x" << std::hex << iova << std::dec << "\n";
    }
    return phys.has_value() == expect_ok;
}

int main() {
    // the driver of the NIC mapped one receive buffer, which the device writes
    Domain nic;
    nic[0x1] = Entry{0x7f3000, true};
    // and one transmit buffer, which the device only needs to read
    nic[0x2] = Entry{0x7f5000, false};
    // the NVMe drive has a domain of its own: the same IOVA leads somewhere else
    Domain nvme;
    nvme[0x1] = Entry{0x2a0000, true};

    bool ok = true;
    std::cout << "domain of the NIC (0000:03:00.0): 2 pages mapped\n";
    ok = dma("03:00.0", nic, 0x1040, true, true) && ok;      // inside the receive buffer: translated
    ok = dma("03:00.0", nic, 0x2010, false, true) && ok;     // reads its transmit buffer: translated
    ok = dma("03:00.0", nic, 0x2010, true, false) && ok;     // writes a read only page: blocked
    ok = dma("03:00.0", nic, 0x9000, true, false) && ok;     // a stale descriptor: never mapped, blocked
    std::cout << "domain of the NVMe drive (0000:01:00.0): 1 page mapped\n";
    ok = dma("01:00.0", nvme, 0x1040, true, true) && ok;     // same IOVA, another domain, another frame
    ok = dma("01:00.0", nvme, 0x2010, false, false) && ok;   // the buffers of the NIC do not exist here

    const auto a = translate(nic, 0x1040, true);
    const auto b = translate(nvme, 0x1040, true);
    ok = ok && a.has_value() && b.has_value() && *a == 0x7f3040 && *b == 0x2a0040;
    std::cout << "same IOVA 0x1040, two domains, two frames: that is the isolation\n";
    std::cout << "without an IOMMU every one of these six DMAs would have reached RAM\n";
    std::cout << (ok ? "model check: OK\n" : "model check: FAILED\n");
    return ok ? 0 : 1;
}
