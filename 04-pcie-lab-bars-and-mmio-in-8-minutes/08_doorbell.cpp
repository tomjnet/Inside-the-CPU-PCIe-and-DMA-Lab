// BARs and MMIO in 8 Minutes: Talking to a Device - slide 8: ring the doorbell in the model
// Build: make 08_doorbell
#include <cstdint>
#include <iostream>

constexpr std::uint32_t RING = 16;

struct Regs {
    std::uint32_t ctrl;
    std::uint32_t status;
    std::uint32_t irq_mask;
    std::uint32_t doorbell;
};
struct Cmd { std::uint32_t opcode; std::uint32_t payload; };
struct Queue {
    Cmd ring[RING] = {};
    std::uint32_t tail = 0;            // owned by the driver
};

// driver side of a doorbell: prepare in RAM, then one MMIO write
void submit(Queue& q, volatile Regs* dev, const Cmd* cmds, int n) {
    for (int i = 0; i < n; ++i) {
        q.ring[q.tail] = cmds[i];           // plain RAM: cheap, cached
        q.tail = (q.tail + 1) % RING;       // the device is not told
    }
    dev->doorbell = q.tail;                 // 1 PCIe crossing, n cmds
}
// device side of the model: runs when the doorbell is written
// while (head != tail) { run(ring[head]); head = (head + 1) % RING; }

struct ToyDevice {
    std::uint32_t head = 0;            // owned by the device
    std::uint64_t executed = 0;
    std::uint64_t checksum = 0;
    void on_doorbell(const Queue& q, const volatile Regs* dev) {
        std::uint32_t tail = dev->doorbell;
        while (head != tail) {
            checksum += static_cast<std::uint64_t>(q.ring[head].opcode) * 1000u + q.ring[head].payload;
            ++executed;
            head = (head + 1) % RING;
        }
    }
};

int main() {
    constexpr int N = 12;
    Cmd cmds[N];
    for (int i = 0; i < N; ++i) cmds[i] = Cmd{static_cast<std::uint32_t>(i % 3 + 1), static_cast<std::uint32_t>(i * 7)};

    // one doorbell per command: every call of submit ends in exactly one doorbell store
    static Regs bar_a;
    Queue qa;
    ToyDevice da;
    int per_command = 0;
    for (int i = 0; i < N; ++i) {
        submit(qa, &bar_a, &cmds[i], 1);
        ++per_command;
        da.on_doorbell(qa, &bar_a);         // the hardware reacts to the write
    }

    // one doorbell per batch
    static Regs bar_b;
    Queue qb;
    ToyDevice db;
    int per_batch = 0;
    submit(qb, &bar_b, cmds, N);
    ++per_batch;
    db.on_doorbell(qb, &bar_b);

    const double crossing_ns = 200.0;       // a typical order of magnitude for one MMIO write, not a measurement
    std::cout << N << " commands, ring of " << RING << " slots\n";
    std::cout << "  ring per command: " << per_command << " doorbell writes, about " << per_command * crossing_ns
              << " ns of link crossings (typical), device executed " << da.executed << "\n";
    std::cout << "  ring per batch:   " << per_batch << " doorbell write,  about " << per_batch * crossing_ns
              << " ns of link crossings (typical), device executed " << db.executed << "\n";
    std::cout << "same work on the device: checksum " << da.checksum << " and " << db.checksum << "\n";
    std::cout << "the trade: a batch waits for its last command, so a latency sensitive driver rings early when idle\n";
    bool ok = da.executed == static_cast<std::uint64_t>(N) && db.executed == da.executed && da.checksum == db.checksum;
    return ok ? 0 : 1;
}
