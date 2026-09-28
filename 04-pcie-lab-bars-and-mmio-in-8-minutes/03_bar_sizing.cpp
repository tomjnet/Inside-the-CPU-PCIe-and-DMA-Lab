// BARs and MMIO in 8 Minutes: Talking to a Device - slide 3: what a bar asks for
// Build: make 03_bar_sizing
#include <cstdint>
#include <iomanip>
#include <iostream>

// A toy BAR, as the device implements it: the low address bits are hard wired to zero, so a write of
// all ones reads back as a mask. size must be a power of two; flags are the read only low bits.
struct ToyBar {
    std::uint32_t size;
    std::uint32_t flags;
    std::uint32_t value;
    void write(std::uint32_t v) { value = (v & ~(size - 1)) | flags; }
    std::uint32_t read() const { return value; }
};

static void decode(const char* name, std::uint32_t readback) {
    bool io = (readback & 0x1) != 0;
    std::uint32_t mask = io ? (readback & ~0x3u) : (readback & ~0xFu);   // an I/O BAR has only 2 flag bits
    std::uint32_t bytes = ~mask + 1;
    std::cout << "  " << std::left << std::setw(16) << name << std::right << " read back 0x" << std::hex
              << std::setw(8) << std::setfill('0') << readback << std::dec << std::setfill(' ') << "  "
              << (io ? "I/O space   " : "memory space") << "  size " << std::setw(9) << bytes << " bytes";
    if (!io) {
        std::cout << (((readback >> 1) & 0x3) == 0x2 ? "  64 bit" : "  32 bit")
                  << ((readback & 0x8) != 0 ? "  prefetchable" : "  not prefetchable");
    }
    std::cout << "\n";
}

int main() {
    // decode what a BAR answers after the sizing write of all ones
    std::uint32_t rb = 0xFFF00004;              // read back, typical
    bool is_io   = (rb & 0x1) != 0;             // bit 0: I/O or memory
    bool is_64   = ((rb >> 1) & 0x3) == 0x2;    // bits 2:1: 64 bit BAR
    bool is_pref = (rb & 0x8) != 0;             // bit 3: prefetchable
    std::uint32_t mask = rb & ~0xFu;            // keep the address bits
    std::uint32_t size = ~mask + 1;             // 0x00100000 = 1 MiB
    // cost: a few configuration accesses per BAR, once, at boot

    std::cout << std::boolalpha << "the BAR of the slide: I/O " << is_io << ", 64 bit " << is_64 << ", prefetchable "
              << is_pref << ", size " << size << " bytes (" << size / (1024 * 1024) << " MiB)\n\n";

    // the device side of the trick: three toy BARs answer the sizing write
    ToyBar nvme{16 * 1024, 0x4, 0};                 // 16 KiB, memory, 64 bit
    ToyBar gpu{256u * 1024 * 1024, 0xC, 0};         // 256 MiB, memory, 64 bit, prefetchable
    ToyBar legacy{32, 0x1, 0};                      // 32 ports, I/O space
    std::cout << "sizing: write 0xFFFFFFFF, read back, invert the address bits, add one\n";
    nvme.write(0xFFFFFFFFu);
    decode("NVMe registers", nvme.read());
    gpu.write(0xFFFFFFFFu);
    decode("GPU aperture", gpu.read());
    legacy.write(0xFFFFFFFFu);
    decode("legacy I/O BAR", legacy.read());

    // then the firmware assigns an address: only the address bits stick
    nvme.write(0xFC500000u);
    std::cout << "\nfirmware writes 0xFC500000 into the NVMe BAR, it reads back 0x" << std::hex << nvme.read() << std::dec
              << " (the address plus the type bits)\n";
    return (size == 0x00100000u && (nvme.read() & ~0xFu) == 0xFC500000u) ? 0 : 1;
}
