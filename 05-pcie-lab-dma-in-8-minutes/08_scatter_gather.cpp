// DMA in 8 Minutes: Devices Move Data Themselves - slide 8: scatter and gather
// Build: make 08_scatter_gather
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

// The model: host RAM is an array of 16 pages, a DMA address is an offset into it plus a base, and the device
// transmits by reading the pieces of the list out of RAM, one DMA read per piece, straight onto the wire.
constexpr std::size_t PAGE = 4096;
constexpr std::uint64_t DMA_BASE = 0x100000000ull;

static std::vector<std::uint8_t> ram(16 * PAGE);
static std::vector<std::uint8_t> wire;          // what left the device
static int dma_reads = 0;

static std::uint64_t dma_map(const std::uint8_t* p) {
    return DMA_BASE + static_cast<std::uint64_t>(p - ram.data());
}

template <class List>                            // the entry type is declared on the slide, inside main
static void device_tx(const List& sg) {
    wire.clear();
    for (const auto& e : sg) {                   // gather: the device walks the list, the CPU does nothing
        auto from = ram.begin() + static_cast<std::ptrdiff_t>(e.dma_addr - DMA_BASE);
        wire.insert(wire.end(), from, from + static_cast<std::ptrdiff_t>(e.len));
        ++dma_reads;
    }
}

int main() {
    // Three buffers in three unrelated places of RAM, as the kernel would find them.
    std::uint8_t* headers = ram.data() + 11 * PAGE + 64;     // built by the network stack
    std::uint8_t* page_a  = ram.data() + 7 * PAGE;           // user payload starts near the end of this page
    std::uint8_t* page_b  = ram.data() + 2 * PAGE;           // and continues here: not adjacent, not even in order
    for (std::size_t i = 0; i < 54; ++i) headers[i] = static_cast<std::uint8_t>(0xA0 + i % 16);
    for (std::size_t i = 0; i < 400; ++i) page_a[3696 + i] = static_cast<std::uint8_t>(i % 251);
    for (std::size_t i = 0; i < 1060; ++i) page_b[i] = static_cast<std::uint8_t>((400 + i) % 251);

    // Scatter/gather: one frame from three buffers, zero memcpy.
    struct SgEntry { std::uint64_t dma_addr; std::uint32_t len; };
    std::vector<SgEntry> sg = {
        {dma_map(headers),       54},    // Ethernet, IP, TCP: the stack
        {dma_map(page_a) + 3696, 400},   // user payload, end of a page
        {dma_map(page_b),        1060},  // and the start of the next
    };
    device_tx(sg);   // the device gathers 3 pieces into 1 frame

    std::cout << "scatter/gather list handed to the device:\n";
    for (const auto& e : sg) std::cout << "  dma_addr=0x" << std::hex << e.dma_addr << std::dec << "  len=" << e.len << "\n";

    // What a driver without scatter/gather would do: copy everything into one bounce buffer first.
    std::vector<std::uint8_t> expected;
    expected.insert(expected.end(), headers, headers + 54);
    expected.insert(expected.end(), page_a + 3696, page_a + 4096);
    expected.insert(expected.end(), page_b, page_b + 1060);

    bool ok = wire == expected && wire.size() == 1514 && dma_reads == 3;
    std::cout << "frame on the wire: " << wire.size() << " bytes from " << dma_reads << " DMA reads\n";
    std::cout << "bytes copied by the CPU with the list: 0; with one bounce buffer: " << expected.size() << "\n";
    std::cout << "payload is continuous across the two pages: byte 453 = " << static_cast<int>(wire[453])
              << ", byte 454 = " << static_cast<int>(wire[454]) << "\n";
    std::cout << (ok ? "every byte of the frame matches\n" : "MISMATCH in the gathered frame\n");
    return ok ? 0 : 1;
}
