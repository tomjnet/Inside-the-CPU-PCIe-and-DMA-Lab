// BARs and MMIO in 8 Minutes: Talking to a Device - slide 7: a register block model
// Build: make 07_register_block
#include <cstddef>
#include <cstdint>
#include <iostream>

int main() {
    // the register block of a toy device, as its data sheet draws it
    struct Regs {                      // offsets inside BAR 0
        std::uint32_t ctrl;            // 0x00 write: enable, reset
        std::uint32_t status;          // 0x04 read: ready, error
        std::uint32_t irq_mask;        // 0x08
        std::uint32_t doorbell;        // 0x0C write: new queue tail
    };
    // a real driver asks the kernel: base = pci_iomap(pdev, 0, 0)
    static Regs bar0;                  // the lab: plain memory, no device
    volatile Regs* dev = &bar0;        // same code shape as the driver
    dev->ctrl = 1;                     // one store = one MMIO write
    std::uint32_t s = dev->status;     // one load  = one MMIO read

    // the layout must match the data sheet byte for byte: check it at compile time
    static_assert(sizeof(Regs) == 16, "four 32 bit registers, no padding");
    static_assert(offsetof(Regs, status) == 0x04, "status at offset 0x04");
    static_assert(offsetof(Regs, irq_mask) == 0x08, "irq_mask at offset 0x08");
    static_assert(offsetof(Regs, doorbell) == 0x0C, "doorbell at offset 0x0C");

    std::cout << "register block: " << sizeof(Regs) << " bytes, layout checked with static_assert\n"
              << "  ctrl     at 0x00\n"
              << "  status   at 0x04\n"
              << "  irq_mask at 0x08\n"
              << "  doorbell at 0x0C\n";
    std::uint32_t ctrl_now = dev->ctrl;
    std::cout << "after the store: ctrl = " << ctrl_now << ", status read = " << s << "\n";
    std::cout << "this base is plain memory at " << static_cast<const void*>(&bar0)
              << "; the base of a driver comes from the kernel and can change from boot to boot\n";
    return ctrl_now == 1 ? 0 : 1;
}
