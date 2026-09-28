// NUMA Aware PCIe in 9 Minutes: NICs, NVMe and GPUs - slide 8: parse local_cpulist
// Build: make 08_parse_cpulist
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

// "0-15,32-47" to 0..15 and 32..47: the format of local_cpulist
std::vector<int> parse_cpulist(const std::string& text) {
    std::vector<int> cpus;
    std::stringstream ss(text);
    for (std::string part; std::getline(ss, part, ',');) {
        int lo = std::stoi(part), hi = lo;       // stoi stops at '-'
        auto dash = part.find('-');              // npos: a single CPU
        if (dash != part.npos) hi = std::stoi(part.substr(dash + 1));
        for (int c = lo; c <= hi; ++c) cpus.push_back(c);
    }
    return cpus;                                 // pin the workers here
}

static void show(const std::string& text) {
    const std::vector<int> cpus = parse_cpulist(text);
    std::cout << "  \"" << text << "\" is " << cpus.size() << " CPUs:";
    const std::size_t head = cpus.size() > 8 ? 6 : cpus.size();   // keep long lists on one line
    for (std::size_t i = 0; i < head; ++i) std::cout << ' ' << cpus[i];
    if (head < cpus.size()) {
        std::cout << " (and " << cpus.size() - head - 1 << " more) " << cpus.back();
    }
    std::cout << "\n";
}

int main() {
    std::cout << "sample strings (typical local_cpulist contents):\n";
    show("0-15,32-47");                          // physical cores, then their hyperthread siblings
    show("16-31,48-63");
    show("0-7");
    show("3");
    show("0,2,4-6");

    int failed = 0;
    const std::vector<int> expect{0, 2, 4, 5, 6};
    if (parse_cpulist("0,2,4-6") != expect) ++failed;
    if (parse_cpulist("0-15,32-47").size() != 32) ++failed;
    if (!parse_cpulist("").empty()) ++failed;
    std::cout << (failed == 0 ? "self check: ok\n" : "self check: FAILED\n");

    // the real file of the first PCIe device, when this machine has one
    namespace fs = std::filesystem;
    std::error_code ec;
    bool found = false;
    for (const auto& dev : fs::directory_iterator("/sys/bus/pci/devices", ec)) {
        std::ifstream f(dev.path() / "local_cpulist");
        std::string line;
        if (f && std::getline(f, line) && !line.empty()) {
            std::cout << "this machine, " << dev.path().filename().string() << ":\n";
            try {
                show(line);
            } catch (const std::exception& e) {
                std::cout << "  could not parse \"" << line << "\": " << e.what() << "\n";
            }
            found = true;
            break;
        }
    }
    if (!found) std::cout << "no local_cpulist file here (Windows, WSL or no PCIe devices): samples only\n";
    return failed == 0 ? 0 : 1;
}
