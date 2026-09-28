// DMA in 8 Minutes: Devices Move Data Themselves - slide 7: driver side: harvest and refill
// Build: make 07_harvest
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
#include <span>
#include <vector>

// The whole NIC receive queue as a model: driver (main), device (device_rx), host RAM (an array).
// Change RING to 4 or 16 and watch the drops of the large burst appear or vanish (the last check expects 8).
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

// A device register that counts its writes: every write is one MMIO store, one PCIe crossing.
struct Doorbell {
    std::uint32_t value = 0;
    int writes = 0;
    Doorbell& operator=(std::uint32_t v) { value = v; ++writes; return *this; }
    operator std::uint32_t() const { return value; }
};
struct Mmio { Doorbell rx_tail; };

static std::vector<std::uint8_t> ram(static_cast<std::size_t>(RING) * BUF_SIZE);
static RxDesc ring[RING];
static Mmio mmio;
static std::uint32_t head = 0;                 // device side index

static std::uint64_t dma_map(const std::uint8_t* p) {
    return DMA_BASE + static_cast<std::uint64_t>(p - ram.data());
}
static const std::uint8_t* ram_at(std::uint64_t dma_addr) {
    return ram.data() + static_cast<std::size_t>(dma_addr - DMA_BASE);
}

static void dma_write(std::uint64_t dma_addr, std::span<const std::uint8_t> data) {
    std::copy(data.begin(), data.end(), ram.begin() + static_cast<std::ptrdiff_t>(dma_addr - DMA_BASE));
}

static bool device_rx(std::span<const std::uint8_t> pkt) {      // slide 6
    if (head == mmio.rx_tail) return false;
    RxDesc& d = ring[head];
    dma_write(d.dma_addr, pkt);
    d.pkt_len = static_cast<std::uint16_t>(pkt.size());
    d.status = DONE;
    head = (head + 1) % RING;
    return true;
}

static std::uint64_t checksum(const std::uint8_t* p, std::size_t n) {
    std::uint64_t sum = 0;
    for (std::size_t i = 0; i < n; ++i) sum = sum * 31 + p[i];
    return sum;
}

static std::uint64_t delivered_sum = 0;
static int delivered = 0;
static bool inside_ram = true;
static void deliver(const std::uint8_t* data, std::uint16_t len) {   // the network stack: reads in place
    if (data < ram.data() || data + len > ram.data() + ram.size()) inside_ram = false;
    delivered_sum += checksum(data, len);
    ++delivered;
}

int main() {
    for (std::uint32_t i = 0; i < RING; ++i) {                        // slide 5: post every buffer
        ring[i] = RxDesc{dma_map(ram.data() + static_cast<std::size_t>(i) * BUF_SIZE), BUF_SIZE, 0, 0};
    }
    std::uint32_t tail = RING - 1;
    std::uint32_t next = 0;                                           // driver side index
    mmio.rx_tail = tail;

    std::mt19937 rng(42);
    std::uint64_t accepted_sum = 0;
    int sent = 0;
    int dropped = 0;
    const int bursts[] = {3, 5, 1, 10, 7, 2};
    std::cout << "burst  accepted  dropped  harvested  head  tail\n";
    for (int burst : bursts) {
        int ok = 0;
        for (int p = 0; p < burst; ++p) {                             // the wire: packets arrive back to back
            std::vector<std::uint8_t> pkt(60 + rng() % 1455);
            for (auto& b : pkt) b = static_cast<std::uint8_t>(rng() & 0xFFu);
            ++sent;
            if (device_rx(pkt)) {
                ++ok;
                accepted_sum += checksum(pkt.data(), pkt.size());
            } else {
                ++dropped;
            }
        }
        int before = delivered;

        // Driver: harvest every DONE slot. Zero copies of the payload.
        while (ring[next].status & DONE) {
            deliver(ram_at(ring[next].dma_addr), ring[next].pkt_len);
            ring[next].status = 0;           // same buffer, back to the device
            tail = next;
            next = (next + 1) % RING;
        }
        mmio.rx_tail = tail;                 // one doorbell for the batch

        std::cout << "  " << burst << (burst < 10 ? "      " : "     ") << ok << "         " << burst - ok
                  << "        " << delivered - before << "          " << head << "     " << tail << "\n";
    }

    std::cout << "sent " << sent << ", delivered " << delivered << ", dropped " << dropped
              << " (a burst larger than the " << RING - 1 << " usable slots overflows this ring)\n";
    std::cout << "doorbells (PCIe crossings by the CPU): " << mmio.rx_tail.writes << " for " << delivered
              << " packets; payload bytes copied by the CPU: 0\n";
    std::cout << "every delivered pointer was inside the pinned buffers: " << (inside_ram ? "yes" : "NO") << "\n";
    bool ok = inside_ram && delivered + dropped == sent && delivered_sum == accepted_sum;
    if (RING == 8 && dropped != 3) ok = false;                        // 10 packets into 7 usable slots
    std::cout << (ok ? "checksums of accepted and delivered packets match\n"
                     : "MISMATCH: a packet was lost or damaged\n");
    return ok ? 0 : 1;
}
