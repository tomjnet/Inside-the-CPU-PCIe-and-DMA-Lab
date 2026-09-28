// IOMMU in 8 Minutes: Translation and Isolation for Devices - slide 8: iommu groups and vfio
// Build: make 08_iommu_groups
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

void print_groups(const fs::path& root) { // /sys/kernel/iommu_groups
    std::error_code ec;                   // no folder: prints nothing
    for (const auto& g : fs::directory_iterator(root, ec)) {
        std::cout << "group " << g.path().filename().string() << ':';
        auto devs = fs::directory_iterator(g.path() / "devices", ec);
        for (const auto& d : devs)        // VFIO takes the whole group
            std::cout << ' ' << d.path().filename().string();  // BDF
        std::cout << '\n';
    }
}

// A BDF as a file name. Windows file names cannot hold a colon, so the practice tree uses an underscore there.
static std::string bdf_name(std::string bdf) {
#if defined(_WIN32)
    for (char& c : bdf) {
        if (c == ':') c = '_';
    }
#endif
    return bdf;
}

static std::size_t count_entries(const fs::path& dir) {
    std::error_code ec;
    std::size_t n = 0;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) ++n;
    return n;
}

int main() {
    // 1. a practice tree with the shape of sysfs, so the reader has something to read on any machine
    std::error_code ec;
    const fs::path fake = fs::temp_directory_path(ec) / "tomjnet_iommu_groups_model";
    fs::remove_all(fake, ec);
    struct Dev { const char* group; const char* bdf; };
    const Dev devs[] = {
        {"13", "0000:01:00.0"},                            // an NVMe drive alone in its group
        {"14", "0000:03:00.0"}, {"14", "0000:03:00.1"},    // a GPU and its audio function: one group
        {"15", "0000:06:00.0"}, {"15", "0000:07:00.0"},    // two devices behind a switch without ACS
    };
    bool built = true;
    for (const Dev& d : devs) {
        const fs::path dir = fake / d.group / "devices";
        fs::create_directories(dir, ec);
        std::ofstream file(dir / bdf_name(d.bdf));         // sysfs holds a link here; an empty file is enough
        built = built && file.good();
    }
    std::cout << "practice tree (typical groups, not this machine), in directory order:\n";
    if (built) {
        print_groups(fake);
    } else {
        std::cout << "could not write under the temporary folder, skipped\n";
    }
#if defined(_WIN32)
    std::cout << "(Windows file names cannot hold a colon: 0000_03_00.0 stands for 0000:03:00.0)\n";
#endif
    std::cout << "group 14 and group 15 hold two devices each: VFIO assigns both or none\n";
    const bool counted = !built || count_entries(fake) == 3;
    fs::remove_all(fake, ec);

    // 2. the real thing
    const fs::path real = "/sys/kernel/iommu_groups";
    std::cout << "\nthis machine, " << real.string() << ":\n";
    if (count_entries(real) == 0) {
        std::cout << "no IOMMU groups visible here (IOMMU off in the firmware, a VM, WSL, or not Linux)\n";
        std::cout << "on real Linux hardware with the IOMMU on: one line per group, as in the practice tree\n";
    } else {
        print_groups(real);
    }
    std::cout << (counted ? "model check: OK\n" : "model check: FAILED\n");
    return counted ? 0 : 1;
}
