// BARs and MMIO in 8 Minutes: Talking to a Device - slide 5: a register is not memory
// Build: make 05_volatile_registers
#include <cstdint>
#include <iostream>

enum : std::uint32_t { CTRL = 0, STATUS = 1, NREGS = 2 };
constexpr std::uint32_t READY = 0x1;
constexpr std::uint32_t IRQ_PENDING = 0x2;

// The "mapped BAR" of the lab is plain memory. A real driver gets this pointer from the kernel (pci_iomap).
static std::uint32_t regs[NREGS] = {0, READY};   // STATUS already says READY, so the wait loop ends

// What plain memory cannot show: a register whose read has a side effect. This toy device clears its
// pending bit when STATUS is read, so two loads in a row give two different answers.
struct ToyDevice {
    std::uint32_t status = READY | IRQ_PENDING;
    int loads = 0;
    std::uint32_t read_status() {
        ++loads;
        std::uint32_t seen = status;
        status = status & ~IRQ_PENDING;          // read to clear
        return seen;
    }
};

int main() {
    // without volatile the compiler may drop or merge these accesses
    volatile std::uint32_t* reg = regs;         // regs: the mapped BAR
    reg[CTRL] = 1;                              // store 1: start a reset
    reg[CTRL] = 0;                              // store 2: not a dead store
    while ((reg[STATUS] & READY) == 0) { }      // a fresh load every pass
    std::uint32_t a = reg[STATUS];              // two loads, maybe two
    std::uint32_t b = reg[STATUS];              // answers: a read can clear
    // every load or store is one PCIe crossing, in program order

    std::cout << "plain memory behind a volatile pointer: a = " << a << ", b = " << b
              << " (memory never changes by itself)\n";
    std::cout << "with volatile the binary keeps 2 stores to CTRL and 3 loads of STATUS;\n"
              << "without it, -O2 may keep 1 store and 1 load. Compare: g++ -O2 -S 05_volatile_registers.cpp\n\n";

    ToyDevice dev;
    std::uint32_t first = dev.read_status();
    std::uint32_t second = dev.read_status();
    std::cout << "toy device with a read to clear bit: first load = " << first << ", second load = " << second
              << ", loads seen by the device = " << dev.loads << "\n";
    std::cout << "a compiler that merged the two loads would have hidden the second answer\n";
    return (first != second && a == b) ? 0 : 1;
}
