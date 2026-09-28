// MSI, MSI-X and Device Queues in 8 Minutes - slide 7: per core load: good against bad
// Build: make 07_core_load
#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

struct Flow { std::uint32_t src, dst; std::uint16_t sport, dport; };

static std::uint32_t flow_hash(const Flow& f) {      // the model of slide 5
    std::uint32_t h = f.src * 2654435761u;
    h ^= f.dst + (h << 6) + (h >> 2);
    h ^= ((std::uint32_t{f.sport} << 16) | f.dport) + (h << 6);
    return h ^ (h >> 15);
}
static int queue_of(const Flow& f, const std::array<int, 128>& table) {
    return table[flow_hash(f) % table.size()];
}

struct Mapping { const char* name; std::array<int, 4> irq_core; };   // slide 6
static const std::array<int, 4> worker_core{0, 1, 2, 3};             // worker q pinned on core q

struct Result { std::array<long, 4> load; long remote; };

static Result run(const Mapping& m, const std::vector<Flow>& packets, const std::array<int, 128>& table) {
    std::array<long, 4> load{};
    long remote = 0;
    // per core load: where does the interrupt of each packet land?
    for (const Flow& f : packets) {
        int q = queue_of(f, table);        // RSS picked the queue
        int core = m.irq_core[q];          // MSI-X vector affinity
        ++load[core];                      // handler runs here
        if (core != worker_core[q])        // data sits in another cache:
            ++remote;                      // the worker starts with misses
    }
    return Result{load, remote};
}

int main() {
    std::array<int, 128> table{};
    for (std::size_t i = 0; i < table.size(); ++i) table[i] = static_cast<int>(i % 4);

    // 100000 packets from 4096 flows, fixed seed so every run and every toolchain does the same work
    std::mt19937 rng(7);
    std::vector<Flow> flows;
    for (int i = 0; i < 4096; ++i) {
        const std::uint32_t src = static_cast<std::uint32_t>(rng());
        const std::uint16_t sport = static_cast<std::uint16_t>(1024u + rng() % 60000u);
        flows.push_back(Flow{src, 0x0A000001u, sport, 443});
    }
    std::vector<Flow> packets;
    packets.reserve(100000);
    for (int i = 0; i < 100000; ++i) packets.push_back(flows[rng() % flows.size()]);

    const std::array<Mapping, 3> mappings{{
        {"good: IRQ of queue q on core q", {0, 1, 2, 3}},
        {"piled: every vector on core 0", {0, 0, 0, 0}},
        {"crossed: balanced, IRQ core is never the worker core", {1, 2, 3, 0}},
    }};

    const long total = static_cast<long>(packets.size());
    bool ok = true;
    std::array<long, 3> remotes{};
    for (std::size_t i = 0; i < mappings.size(); ++i) {
        const Result r = run(mappings[i], packets, table);
        std::cout << mappings[i].name << "\n";
        long sum = 0;
        for (std::size_t c = 0; c < r.load.size(); ++c) {
            const double pct = 100.0 * static_cast<double>(r.load[c]) / static_cast<double>(total);
            std::cout << "  core " << c << ": " << std::setw(6) << r.load[c] << " interrupts  " << std::fixed
                      << std::setprecision(1) << std::setw(5) << pct << "%  "
                      << std::string(static_cast<std::size_t>(pct / 2.5), '#') << "\n";
            sum += r.load[c];
        }
        std::cout << "  remote packets (IRQ core is not the worker core): " << r.remote << " of " << total << " = "
                  << 100.0 * static_cast<double>(r.remote) / static_cast<double>(total) << "%\n\n";
        ok = ok && sum == total;               // the model loses no packet
        remotes[i] = r.remote;
    }
    ok = ok && remotes[0] == 0 && remotes[2] == total;   // good is all local, crossed is all remote

    std::cout << "Typical cost of a remote packet: its descriptor and headers sit in the cache of the IRQ core,\n"
                 "so the worker pays tens of nanoseconds per cache line instead of about one. Even load is not\n"
                 "locality: crossed looks as balanced as good and is remote for every packet.\n";
    return ok ? 0 : 1;
}
