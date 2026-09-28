// DMA in 8 Minutes: Devices Move Data Themselves - slide 6: device side: a packet arrives
// Build: make 06_device_rx
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
#include <span>
#include <vector>

// The model: host RAM is an array, a DMA address is an offset into it plus a base (never a pointer), and the
// device is the function below. On real hardware that function is silicon: no CPU instruction runs for it.
constexpr std::uint32_t RING = 8;
constexpr std::uint16_t BUF_SIZE = 2048;
constexpr std::uint64_t DMA_BASE = 0x100000000ull;
constexpr std::uint32_t DONE = 1u;

struct RxDesc {
    std::uint64_t dma_addr;
    std::uint16_t buf_len;
    std::uint16_t pkt_len;
    std::uint32_t status;
};
struct Mmio { std::uint32_t rx_tail; };

static std::vector<std::uint8_t> ram(static_cast<std::size_t>(RING) * BUF_SIZE);
static RxDesc ring[RING];
static Mmio mmio;
static std::uint32_t head = 0;          // device register: next slot the device fills
static long long dma_bytes = 0;         // bytes the device wrote into host RAM by itself

static void dma_write(std::uint64_t dma_addr, std::span<const std::uint8_t> data) {
    std::size_t offset = static_cast<std::size_t>(dma_addr - DMA_BASE);
    if (offset + data.size() > ram.size()) return;       // a real device would corrupt memory here: see the IOMMU
    std::copy(data.begin(), data.end(), ram.begin() + static_cast<std::ptrdiff_t>(offset));
    dma_bytes += static_cast<long long>(data.size());
}

// Device: a packet arrived on the wire. The CPU runs nothing here.
bool device_rx(std::span<const std::uint8_t> pkt) {
    if (head == mmio.rx_tail) return false;  // no buffer: drop
    RxDesc& d = ring[head];                  // DMA read, 16 bytes
    dma_write(d.dma_addr, pkt);              // DMA write, the payload
    d.pkt_len = static_cast<std::uint16_t>(pkt.size());
    d.status  = DONE;                        // ownership: driver
    head = (head + 1) % RING;
    return true;  // then one MSI-X interrupt, or none when polled
}

int main() {
    for (std::uint32_t i = 0; i < RING; ++i) {
        ring[i] = RxDesc{DMA_BASE + i * static_cast<std::uint64_t>(BUF_SIZE), BUF_SIZE, 0, 0};
    }
    mmio.rx_tail = RING - 1;                 // the driver posted the buffers (slide 5)

    std::mt19937 rng(42);
    int accepted = 0;
    int dropped = 0;
    std::cout << "9 packets arrive and the driver never harvests:\n";
    for (int p = 0; p < 9; ++p) {
        std::vector<std::uint8_t> pkt(60 + rng() % 1455);            // 60 to 1514 bytes
        for (auto& b : pkt) b = static_cast<std::uint8_t>(rng() & 0xFFu);
        std::uint32_t slot = head;
        bool ok = device_rx(pkt);
        if (ok) {
            ++accepted;
            auto in_ram = ram.begin() + static_cast<std::ptrdiff_t>(slot) * BUF_SIZE;
            bool same = std::equal(pkt.begin(), pkt.end(), in_ram);
            std::cout << "  packet " << p << ": " << pkt.size() << " bytes, slot " << slot << ", pkt_len="
                      << ring[slot].pkt_len << ", DONE=" << (ring[slot].status & DONE) << ", bytes in host RAM "
                      << (same ? "match" : "DIFFER") << ", head=" << head << "\n";
            if (!same) return 1;
        } else {
            ++dropped;
            std::cout << "  packet " << p << ": " << pkt.size() << " bytes, DROPPED: head == tail == " << head
                      << " (the rx_no_buffer counter of a real NIC)\n";
        }
    }
    std::cout << "accepted " << accepted << ", dropped " << dropped << ", " << dma_bytes
              << " bytes moved by the device, 0 by the CPU\n";
    std::cout << "a ring of " << RING << " slots holds " << RING - 1
              << " packets: one slot stays out so that full and empty differ\n";
    return accepted == 7 && dropped == 2 ? 0 : 1;
}
