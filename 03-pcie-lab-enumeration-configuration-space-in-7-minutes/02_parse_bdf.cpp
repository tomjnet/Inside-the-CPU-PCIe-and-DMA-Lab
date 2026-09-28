// PCIe Enumeration and Configuration Space in 7 Minutes - slide 2: an address for every function: bdf
// Build: make 02_parse_bdf
#include <cctype>
#include <cstddef>
#include <iostream>
#include <optional>
#include <string>

// Helper the slide assumes: n hex digits of s, starting at `at`, as a number. The caller checked the digits.
static unsigned hex(const std::string& s, std::size_t at, std::size_t n) {
    unsigned value = 0;
    for (std::size_t i = at; i < at + n; ++i) {
        const char c = s[i];
        const unsigned digit = (c >= '0' && c <= '9') ? static_cast<unsigned>(c - '0')
                                                      : static_cast<unsigned>(std::tolower(static_cast<unsigned char>(c)) - 'a') + 10u;
        value = value * 16u + digit;
    }
    return value;
}

// Added for the sample: every position that is not a separator must be a hex digit.
static bool all_hex(const std::string& s) {
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (i == 4 || i == 7 || i == 10) continue;
        if (std::isxdigit(static_cast<unsigned char>(s[i])) == 0) return false;
    }
    return true;
}

// "0000:03:00.0" = domain:bus:device.function, all hex
struct Bdf { unsigned domain, bus, device, function; };
std::optional<Bdf> parse_bdf(const std::string& s) {
    if (s.size() != 12 || s[4] != ':' || s[7] != ':' || s[10] != '.')
        return std::nullopt;             // fixed shape: 12 characters
    if (!all_hex(s)) return std::nullopt;   // sample only: the slide assumes hex digits
    Bdf b{hex(s, 0, 4), hex(s, 5, 2), hex(s, 8, 2), hex(s, 11, 1)};
    if (b.device > 31 || b.function > 7) return std::nullopt;
    return b;                            // 16 + 8 + 5 + 3 bits
}

int main() {
    struct Case { const char* text; bool valid; };
    const Case cases[] = {
        {"0000:03:00.0", true},    // the NIC of the slides
        {"0000:01:00.1", true},    // function 1: the audio controller of a GPU
        {"0000:00:1c.0", true},    // device 0x1c = 28 on bus 0: a root port
        {"03:00.0", false},        // the short form lspci prints: add the domain first
        {"0000:03:20.0", false},   // device 0x20 = 32: only 5 bits, 0 to 31
        {"0000:03:00.8", false},   // function 8: only 3 bits, 0 to 7
        {"0000:zz:00.0", false},   // not hex
    };

    int wrong = 0;
    for (const Case& c : cases) {
        const std::optional<Bdf> b = parse_bdf(c.text);
        std::cout << c.text << "  ->  ";
        if (b) {
            // bus, device and function packed the way the hardware routes them: 8 + 5 + 3 = 16 bits.
            const unsigned routing_id = (b->bus << 8) | (b->device << 3) | b->function;
            // Where the 4096 bytes of this function sit inside the memory mapped configuration window (ECAM).
            const unsigned long long ecam = (static_cast<unsigned long long>(b->bus) << 20)
                                            | (static_cast<unsigned long long>(b->device) << 15)
                                            | (static_cast<unsigned long long>(b->function) << 12);
            std::cout << "domain " << b->domain << ", bus " << b->bus << ", device " << b->device << ", function "
                      << b->function << "   routing ID 0x" << std::hex << routing_id << ", ECAM offset 0x" << ecam
                      << std::dec << "\n";
        } else {
            std::cout << "rejected\n";
        }
        if (b.has_value() != c.valid) ++wrong;
    }

    std::cout << "\nlimits: 65536 domains, 256 buses per domain, 32 devices per bus, 8 functions per device\n";
    std::cout << "on PCIe a link has one device at the far end, so the device field is almost always 00\n";
    if (wrong != 0) {
        std::cout << "FAILED: " << wrong << " addresses were classified wrongly\n";
        return 1;
    }
    std::cout << "all " << (sizeof(cases) / sizeof(cases[0])) << " addresses classified as expected\n";
    return 0;
}
