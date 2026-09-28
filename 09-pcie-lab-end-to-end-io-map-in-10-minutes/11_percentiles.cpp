// Build an End to End Low Latency I/O Map in 10 Minutes - slide 11: then measure: the median and the tail
// Build: make 11_percentiles
//
// Times every request, keeps every sample and reads p50, p99 and p99.9 by rank, before and after one change.
// The request is a stand in for your I/O path: a 4 KiB read of a temporary file. Before: open, read and close
// on every request. After: the file stays open, seek and read. The numbers belong to this machine only and
// the program never asserts on them. With 4000 samples p99.9 rests on the 4 slowest requests: on a real
// audit run the real load for minutes, on real Linux hardware.
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <system_error>
#include <vector>

using Clock = std::chrono::steady_clock;

static volatile std::uint64_t g_sink = 0;
static void sink(std::uint64_t x) { g_sink = g_sink + x; }   // keeps the result alive: the loop cannot be deleted

static double elapsed_ns(Clock::time_point t0) {
    return std::chrono::duration<double, std::nano>(Clock::now() - t0).count();
}

static std::uint64_t checksum(const std::vector<char>& buf, std::streamsize got) {
    std::uint64_t sum = 0;
    for (std::size_t i = 0; i < static_cast<std::size_t>(got); ++i) sum += static_cast<unsigned char>(buf[i]);
    return sum;
}

template <class Request>
static void measure(const char* label, Request one_request) {
    const int n = 4000;
    std::vector<double> ns;
    ns.reserve(static_cast<std::size_t>(n));
    auto report = [&](double p50, double p99, double p999) {
        std::cout << "  " << std::left << std::setw(36) << label << std::right << std::fixed << std::setprecision(0)
                  << std::setw(10) << p50 << std::setw(10) << p99 << std::setw(10) << p999 << std::setprecision(1)
                  << std::setw(11) << (p50 > 0.0 ? p999 / p50 : 0.0) << "x\n";
    };
    for (int i = 0; i < 200; ++i) sink(one_request());        // warm up: page cache, allocator, branch predictor

    // time every request, keep every sample: the tail is the finding
    for (int i = 0; i < n; ++i) {
        auto t0 = Clock::now();
        sink(one_request());                   // the path under test
        ns.push_back(elapsed_ns(t0));
    }
    std::sort(ns.begin(), ns.end());           // rank = percentile
    auto pct = [&](double p) { return ns[std::size_t(p * (n - 1))]; };
    report(pct(0.50), pct(0.99), pct(0.999));  // before, then after
}

int main() {
    std::error_code ec;
    std::filesystem::path file = std::filesystem::temp_directory_path(ec);
    if (ec) file = ".";
    file /= "tomjnet_pcie_lab_09_percentiles.bin";
    {
        std::ofstream out(file, std::ios::binary);
        std::vector<char> block(4096, 'x');
        out.write(block.data(), static_cast<std::streamsize>(block.size()));
    }

    std::vector<char> buf(4096);
    std::cout << "4000 requests each, nanoseconds, this machine        p50       p99     p99.9  p99.9/p50\n";

    measure("before: open, read 4 KiB, close", [&] {
        std::ifstream in(file, std::ios::binary);
        in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        return checksum(buf, in.gcount());
    });

    std::ifstream kept(file, std::ios::binary);
    measure("after: file kept open, seek, read", [&] {
        kept.clear();
        kept.seekg(0);
        kept.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        return checksum(buf, kept.gcount());
    });
    kept.close();

    std::filesystem::remove(file, ec);
    std::cout << "the median describes the path (fewer system calls after the change), the tail describes the\n"
                 "collisions (the scheduler, an interrupt, another process): compare both, one change at a time\n";
    return 0;
}
