// DMA in 8 Minutes: Devices Move Data Themselves - slide 5: driver side: post buffers, ring the doorbell
// Build: make 05_post_buffers
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

// The model. Host RAM is an array of bytes. A DMA address is NOT a pointer: it is what the device puts on the
// bus. Here it is the offset into RAM plus a base, so mixing the two up is visible at once. A real driver gets
// it from the kernel (dma_map_single), and behind an IOMMU it is an I/O virtual address (next episode).
constexpr std::uint32_t RING = 8;
constexpr std::uint16_t BUF_SIZE = 2048;
constexpr std::uint64_t DMA_BASE = 0x100000000ull;

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

static std::uint64_t dma_map(const std::uint8_t* p) {
    return DMA_BASE + static_cast<std::uint64_t>(p - ram.data());
}
static const std::uint8_t* ram_at(std::uint64_t dma_addr) {
    return ram.data() + static_cast<std::size_t>(dma_addr - DMA_BASE);
}

int main() {
    std::vector<std::uint8_t*> buffers;
    for (std::uint32_t i = 0; i < RING; ++i) buffers.push_back(ram.data() + static_cast<std::size_t>(i) * BUF_SIZE);
    std::uint32_t tail = 0;

    // Driver: give every slot a pinned buffer, then ring the doorbell.
    // Cost: one MMIO write, one PCIe crossing, for the whole batch.
    for (std::uint32_t i = 0; i < RING; ++i) {
        ring[i].dma_addr = dma_map(buffers[i]);  // pin, get bus address
        ring[i].buf_len  = BUF_SIZE;
        ring[i].status   = 0;                    // not DONE yet
    }
    tail = RING - 1;                // head == tail must mean "no buffer"
    mmio.rx_tail = tail;            // doorbell: slots below tail are yours

    std::cout << "slot  pointer (CPU, virtual)  dma_addr (device, bus)  buf_len  status\n";
    bool ok = true;
    for (std::uint32_t i = 0; i < RING; ++i) {
        std::cout << "  " << i << "   " << static_cast<const void*>(buffers[i]) << "        0x" << std::hex
                  << ring[i].dma_addr << std::dec << "             " << ring[i].buf_len << "     "
                  << ring[i].status << "\n";
        if (ram_at(ring[i].dma_addr) != buffers[i]) ok = false;     // the mapping must lead back to the buffer
    }
    std::uint32_t usable = static_cast<std::uint32_t>(mmio.rx_tail);   // head is 0: slots 0 to tail - 1
    std::cout << "rx_tail register = " << usable << ": the device may fill " << usable << " of " << RING
              << " slots before it must drop\n";
    std::cout << "MMIO writes (PCIe crossings by the CPU) for " << RING << " buffers: " << mmio.rx_tail.writes << "\n";
    std::cout << "pinned memory handed to the device: " << RING * static_cast<std::uint32_t>(BUF_SIZE) << " bytes\n";
    std::cout << (ok ? "every dma_addr maps back to its buffer\n" : "MISMATCH between dma_addr and buffer\n");
    return ok && mmio.rx_tail.writes == 1 ? 0 : 1;
}
