// PCIe Enumeration and Configuration Space in 7 Minutes - slide 6: read the identity from sysfs
// Build: make 06_sysfs_reader
//
// Portable C++20 only: std::filesystem and std::ifstream. On Windows, and on a Linux with no PCIe devices in
// sysfs (WSL shows few or none), the program says so and exits 0.
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

// Helper the slide assumes: a sysfs identity file holds one number such as 0x8086. 0 when it cannot be read.
static unsigned read_hex(const std::filesystem::path& file) {
    std::ifstream in(file);
    std::string text;
    if (!(in >> text)) return 0;
    try {
        return static_cast<unsigned>(std::stoul(text, nullptr, 16));
    } catch (const std::exception&) {
        return 0;
    }
}

// The decoder of slide 4: the top byte of the class code picks the family.
struct ClassName { unsigned base; const char* name; };
constexpr ClassName kClasses[] = {
    {0x01, "mass storage"}, {0x02, "network"}, {0x03, "display"}, {0x04, "multimedia"},
    {0x05, "memory"},       {0x06, "bridge"},  {0x08, "system peripheral"}, {0x0c, "serial bus"}};

static const char* class_name(unsigned cc) {
    for (const ClassName& c : kClasses)
        if (c.base == (cc >> 16)) return c.name;
    return "other";
}

int main() {
    const char* root = "/sys/bus/pci/devices";
    std::error_code ec;
    int rows = 0;

    if (std::filesystem::is_directory(root, ec)) {
        std::cout << "BDF           vendor:device  class\n";
        try {
            // one directory per function: /sys/bus/pci/devices/0000:03:00.0
            // the kernel filled these files at enumeration: no PCIe crossing
            namespace fs = std::filesystem;
            for (const auto& e : fs::directory_iterator("/sys/bus/pci/devices")) {
                std::string bdf = e.path().filename().string();
                unsigned vendor = read_hex(e.path() / "vendor");   // 0x8086
                unsigned device = read_hex(e.path() / "device");   // 0x1533
                unsigned cc     = read_hex(e.path() / "class");    // 0x020000
                std::cout << bdf << "  " << std::hex << vendor << ':' << device
                          << "  " << class_name(cc) << '\n';
                ++rows;
            }
            // missing or empty directory: no PCIe devices visible here (WSL)
        } catch (const std::filesystem::filesystem_error& err) {
            std::cout << "stopped reading " << root << ": " << err.code().message() << "\n";
        }
        std::cout << std::dec;
    }

    if (rows == 0) {
        std::cout << "no PCIe devices visible here (WSL, a container or not Linux): " << root
                  << " is missing or empty\n";
        std::cout << "on real Linux hardware this prints one row per function, for example:\n";
        std::cout << "  0000:03:00.0  8086:1533  network\n";
        std::cout << "  0000:04:00.0  144d:a808  mass storage\n";
    } else {
        std::cout << rows << " functions, read from files the kernel cached: no root, no driver, no PCIe crossing\n";
    }
    return 0;
}
