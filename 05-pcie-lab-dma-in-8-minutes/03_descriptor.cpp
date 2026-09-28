// DMA in 8 Minutes: Devices Move Data Themselves - slide 3: pinned buffers, dma addresses, descriptors
// Build: make 03_descriptor
#include <cstddef>
#include <cstdint>
#include <iostream>

constexpr std::uint32_t RING = 8;   // a real NIC ring holds 256 to 4096 descriptors

int main() {
    // One RX descriptor: what the device reads to find a buffer.
    struct RxDesc {
        std::uint64_t dma_addr;  // bus address of a pinned buffer
        std::uint16_t buf_len;   // room in that buffer, in bytes
        std::uint16_t pkt_len;   // written by the device: bytes used
        std::uint32_t status;    // bit 0 = DONE, written by the device
    };
    constexpr std::uint32_t DONE = 1u;
    static_assert(sizeof(RxDesc) == 16);  // 4 descriptors per cache line
    RxDesc ring[RING];                    // coherent memory, shared

    // The driver writes the first two fields, the device writes the last two.
    for (std::uint32_t i = 0; i < RING; ++i) ring[i] = RxDesc{0x100000000ull + i * 2048ull, 2048, 0, 0};
    ring[0].pkt_len = 1514;               // what the device would write back
    ring[0].status = DONE;

    std::cout << "RxDesc layout (the device and the driver must agree on every byte)\n";
    std::cout << "  field      offset  size  written by\n";
    std::cout << "  dma_addr   " << offsetof(RxDesc, dma_addr) << "       8     driver\n";
    std::cout << "  buf_len    " << offsetof(RxDesc, buf_len) << "       2     driver\n";
    std::cout << "  pkt_len    " << offsetof(RxDesc, pkt_len) << "      2     device\n";
    std::cout << "  status     " << offsetof(RxDesc, status) << "      4     device\n";
    std::cout << "sizeof(RxDesc) = " << sizeof(RxDesc) << " bytes, " << 64 / sizeof(RxDesc)
              << " descriptors per 64 byte cache line\n";
    std::cout << "this ring: " << RING << " slots = " << sizeof(ring) << " bytes\n";
    std::cout << "a 512 slot ring = " << 512 * sizeof(RxDesc) << " bytes, a 4096 slot ring = "
              << 4096 * sizeof(RxDesc) << " bytes of coherent memory\n";

    std::cout << "slot 0 after one packet: dma_addr=0x" << std::hex << ring[0].dma_addr << std::dec
              << " buf_len=" << ring[0].buf_len << " pkt_len=" << ring[0].pkt_len
              << " DONE=" << ((ring[0].status & DONE) ? "yes" : "no") << "\n";
    std::cout << "slot 1, still waiting:   dma_addr=0x" << std::hex << ring[1].dma_addr << std::dec
              << " DONE=" << ((ring[1].status & DONE) ? "yes" : "no") << "\n";
    return 0;
}
