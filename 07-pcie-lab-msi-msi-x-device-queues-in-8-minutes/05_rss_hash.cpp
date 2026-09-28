// MSI, MSI-X and Device Queues in 8 Minutes - slide 5: rss: hash the flow, pick the queue
// Build: make 05_rss_hash
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

struct Flow { std::uint32_t src, dst; std::uint16_t sport, dport; };
// RSS: the NIC hashes the flow in hardware. O(1), no lock, no CPU work
std::uint32_t flow_hash(const Flow& f) {      // a model of Toeplitz
    std::uint32_t h = f.src * 2654435761u;
    h ^= f.dst + (h << 6) + (h >> 2);
    h ^= ((std::uint32_t{f.sport} << 16) | f.dport) + (h << 6);
    return h ^ (h >> 15);
}
// indirection table (ethtool -x): the hash picks an entry, a queue
int queue_of(const Flow& f, const std::array<int, 128>& table) {
    return table[flow_hash(f) % table.size()];  // same flow, same queue
}

int main() {
    constexpr int kQueues = 4;
    std::array<int, 128> table{};
    for (std::size_t i = 0; i < table.size(); ++i) table[i] = static_cast<int>(i % kQueues);   // 0 1 2 3 0 1 2 3

    // 256 flows: clients on random addresses and ports, one server on port 443. Fixed seed: same work everywhere.
    std::mt19937 rng(7);
    std::vector<Flow> flows;
    for (int i = 0; i < 256; ++i) {
        const std::uint32_t src = static_cast<std::uint32_t>(rng());
        const std::uint16_t sport = static_cast<std::uint16_t>(1024u + rng() % 60000u);
        flows.push_back(Flow{src, 0x0A000001u, sport, 443});
    }

    // property 1: every packet of one flow lands on the same queue
    bool stable = true;
    for (const Flow& f : flows) {
        const int first = queue_of(f, table);
        for (int packet = 0; packet < 10; ++packet) stable = stable && queue_of(f, table) == first;
    }
    std::cout << "same flow, same queue for 256 flows x 10 packets: " << (stable ? "yes" : "NO") << "\n";

    // property 2: many flows spread across the queues
    std::array<int, kQueues> flows_on{};
    for (const Flow& f : flows) ++flows_on[static_cast<std::size_t>(queue_of(f, table))];
    std::cout << "flows per queue (256 flows, 4 queues, 64 would be perfect):\n";
    for (std::size_t q = 0; q < flows_on.size(); ++q) std::cout << "  queue " << q << ": " << flows_on[q] << "\n";

    // the limit: one heavy flow is still one queue, RSS balances flows, not packets
    const Flow& heavy = flows[0];
    std::cout << "one heavy flow of 1000000 packets: all of them on queue " << queue_of(heavy, table)
              << ", the other " << kQueues - 1 << " queues do not help\n";

    const Flow& f1 = flows[1];
    std::cout << "example: hash 0x" << std::hex << flow_hash(f1) << std::dec << " -> table entry "
              << flow_hash(f1) % table.size() << " -> queue " << queue_of(f1, table) << "\n";
    return stable ? 0 : 1;
}
