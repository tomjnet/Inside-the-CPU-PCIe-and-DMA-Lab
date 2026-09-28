// PCIe Enumeration and Configuration Space in 7 Minutes - slide 5: capabilities: a linked list inside the space
// Build: make 05_capabilities
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>

using Config = std::array<std::uint8_t, 256>;

// Helper the slide assumes: the names of the capability IDs a viewer meets first.
static const char* cap_name(unsigned id) {
    switch (id) {
        case 0x01: return "power management";
        case 0x05: return "MSI";
        case 0x10: return "PCI Express (LnkCap and LnkSta live here)";
        case 0x11: return "MSI-X";
        default:   return "(another capability)";
    }
}

// cfg: the 256 bytes of one function. Status bit 4: list present
void walk_capabilities(const std::array<std::uint8_t, 256>& cfg) {
    if ((cfg[0x06] & 0x10) == 0) return;     // no capability list
    for (unsigned p = cfg[0x34] & 0xfcu; p; p = cfg[p + 1] & 0xfcu) {
        unsigned id = cfg[p];                // byte 0: ID, byte 1: next
        std::cout << "cap 0x" << std::hex << id << " at 0x" << p
                  << "  " << cap_name(id) << '\n';  // 0x10: PCIe
    }                                        // next == 0 ends the list
}

// The same walk as a counter, with the guard a real tool adds: a broken device must not loop forever.
static int count_capabilities(const Config& cfg) {
    if ((cfg[0x06] & 0x10) == 0) return 0;
    int count = 0;
    for (unsigned p = cfg[0x34] & 0xfcu; p != 0 && count < 48; p = cfg[p + 1] & 0xfcu) ++count;
    return count;
}

// Configuration space is little endian: the low byte sits at the lower offset.
static void put16(Config& cfg, std::size_t at, unsigned value) {
    cfg[at] = static_cast<std::uint8_t>(value & 0xffu);
    cfg[at + 1] = static_cast<std::uint8_t>((value >> 8) & 0xffu);
}
static unsigned get16(const Config& cfg, std::size_t at) {
    return static_cast<unsigned>(cfg[at]) | (static_cast<unsigned>(cfg[at + 1]) << 8);
}
static void put_cap(Config& cfg, std::size_t at, unsigned id, unsigned next) {
    cfg[at] = static_cast<std::uint8_t>(id);
    cfg[at + 1] = static_cast<std::uint8_t>(next);
}

// A model of the NIC of the slides: the numbers are typical, nothing here reads a real device.
static Config model_nic() {
    Config cfg{};
    put16(cfg, 0x00, 0x8086);        // vendor ID: Intel
    put16(cfg, 0x02, 0x1533);        // device ID: I210
    put16(cfg, 0x06, 0x0010);        // status: bit 4 set, the capability list exists
    cfg[0x09] = 0x00;                // class code 0x020000, low byte first: prog IF
    cfg[0x0a] = 0x00;                //   subclass: Ethernet
    cfg[0x0b] = 0x02;                //   base class: network
    cfg[0x0e] = 0x00;                // header type 0: an endpoint, one function
    cfg[0x34] = 0x40;                // capabilities pointer
    put_cap(cfg, 0x40, 0x01, 0x50);  // power management
    put_cap(cfg, 0x50, 0x05, 0x70);  // MSI
    put_cap(cfg, 0x70, 0x10, 0xb0);  // PCI Express
    put_cap(cfg, 0xb0, 0x11, 0x00);  // MSI-X, next == 0: end of the list
    return cfg;
}

int main() {
    const Config nic = model_nic();
    const unsigned cc = (static_cast<unsigned>(nic[0x0b]) << 16) | (static_cast<unsigned>(nic[0x0a]) << 8) | nic[0x09];

    std::cout << "model of one function (typical values, not read from hardware)\n";
    std::cout << std::hex << "vendor 0x" << get16(nic, 0x00) << ", device 0x" << get16(nic, 0x02) << ", class 0x" << cc
              << ", header type 0x" << static_cast<unsigned>(nic[0x0e]) << ", cap ptr 0x"
              << static_cast<unsigned>(nic[0x34]) << std::dec << "\n\n";

    walk_capabilities(nic);
    std::cout << std::dec;
    const int found = count_capabilities(nic);
    std::cout << found << " capabilities in the list\n\n";

    Config legacy = nic;
    put16(legacy, 0x06, 0x0000);     // status bit 4 clear: the pointer at 0x34 must be ignored
    std::cout << "same bytes with status bit 4 cleared:\n";
    walk_capabilities(legacy);
    std::cout << std::dec;
    const int none = count_capabilities(legacy);
    std::cout << none << " capabilities: the walk never reads 0x34\n";

    if (found != 4 || none != 0) {
        std::cout << "FAILED: expected 4 and 0 capabilities\n";
        return 1;
    }
    return 0;
}
