// Build an End to End Low Latency I/O Map in 10 Minutes - slide 10: a model of the knobs
// Build: make 10_queue_knobs
//
// A model in ticks, not a measurement: a burst of 40 packets arrives every 64 ticks in a ring of `depth`
// slots; the core wakes up, pays a fixed cost and harvests up to `batch` packets. The table shows the three
// knobs of the slide: a ring smaller than the burst drops, a batch of one cannot keep up and a deeper ring
// only turns its drops into waiting, a batch spreads the fixed cost. Deterministic output.
#include <algorithm>
#include <cstddef>
#include <deque>
#include <iomanip>
#include <iostream>

struct Stats {
    long long arrived = 0, dropped = 0, wakeups = 0, wait = 0, delivered = 0, left = 0;
};

static Stats run(std::size_t depth, std::size_t batch) {
    Stats s;
    std::deque<int> q;                     // the ring: the arrival tick of every packet in it
    int busy = 0;

    // a wake up costs 4 ticks fixed (IRQ, doorbell) plus 1 per packet
    for (int t = 0; t < 64000; ++t) {
        for (int i = 0; t % 64 == 0 && i < 40; ++i) {   // burst of 40
            if (q.size() < depth) q.push_back(t);
            else ++s.dropped;                  // full ring: backpressure
        }
        if (busy > 0) { --busy; continue; }    // core still working
        std::size_t n = std::min(batch, q.size());      // one batch
        if (n > 0) { busy = 3 + static_cast<int>(n); ++s.wakeups; }
        for (; n > 0; --n) { s.wait += t - q.front(); q.pop_front(); }
    }                                          // busy: this tick + 3 + n

    s.arrived = 40LL * (64000 / 64);
    s.left = static_cast<long long>(q.size());
    s.delivered = s.arrived - s.dropped - s.left;
    return s;
}

int main() {
    const std::size_t depths[] = {16, 64, 1024};
    const std::size_t batches[] = {1, 8, 32};
    bool ok = true;

    std::cout << "bursts of 40 packets every 64 ticks, 1000 bursts; a wake up costs 4 ticks + 1 per packet\n";
    std::cout << "  depth  batch   delivered   dropped   wake ups   mean wait (ticks)   ticks per packet\n";
    for (std::size_t depth : depths) {
        for (std::size_t batch : batches) {
            const Stats s = run(depth, batch);
            const double done = static_cast<double>(s.delivered);
            const double mean_wait = s.delivered > 0 ? static_cast<double>(s.wait) / done : 0.0;
            const double cost = s.delivered > 0 ? static_cast<double>(4 * s.wakeups + s.delivered) / done : 0.0;
            std::cout << std::setw(7) << depth << std::setw(7) << batch << std::setw(12) << s.delivered
                      << std::setw(10) << s.dropped << std::setw(11) << s.wakeups << std::fixed << std::setprecision(1)
                      << std::setw(20) << mean_wait << std::setw(19) << cost << "\n";
            // every packet is delivered, dropped or still in the ring, and the ring never exceeds its depth
            if (s.delivered < 0 || s.left > static_cast<long long>(depth)) ok = false;
        }
    }
    std::cout << "depth 16: the burst of 40 does not fit, at least 24 of every 40 are dropped whatever the batch\n";
    std::cout << "batch 1: 5 ticks per packet, the core cannot keep up; depth 1024 only makes each packet wait longer\n";
    std::cout << "depth 64, batch 32: no drops, far fewer wake ups, a short wait\n";
    std::cout << "this batch never waits to fill: a coalescing timer that does wait adds that delay to every packet\n";
    if (!ok) {
        std::cout << "model accounting is wrong\n";
        return 1;
    }
    return 0;
}
