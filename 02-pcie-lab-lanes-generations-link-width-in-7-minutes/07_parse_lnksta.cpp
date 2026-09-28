// PCIe Lanes, Generations and Link Width in 7 Minutes - slide 7: flag a down trained link in c++
// Build: make 07_parse_lnksta
// Parses the LnkCap and LnkSta lines of lspci -vv and reports the bandwidth kept and lost.
// Usage: ./07_parse_lnksta                                 sample lines (WSL shows no PCIe devices)
//        ./07_parse_lnksta "LnkCap line" "LnkSta line"     the two lines of a real device
#include <cstdio>
#include <exception>
#include <iostream>
#include <string>

struct Link { double gt_s = 0; int width = 0; };

Link parse_link(const std::string& line) {   // "Speed 8GT/s, Width x4"
    Link l;                                  // one pass over one line
    auto s = line.find("Speed "), w = line.find("Width x");
    if (s != line.npos) l.gt_s = std::stod(line.substr(s + 6));
    if (w != line.npos) l.width = std::stoi(line.substr(w + 7));
    return l;
}
bool down_trained(const Link& cap, const Link& sta) {
    return sta.gt_s < cap.gt_s || sta.width < cap.width;  // slower link
}

// the calculator of slide 4: 8b/10b up to 5 GT/s (Gen 1 and 2), 128b/130b from 8 GT/s (Gen 3 to 5)
static double link_gb_s(const Link& l) {
    const double encoding = l.gt_s <= 5.0 ? 8.0 / 10.0 : 128.0 / 130.0;
    return l.gt_s * encoding * l.width / 8.0;
}

static bool report(const std::string& name, const std::string& cap_line, const std::string& sta_line) {
    const Link cap = parse_link(cap_line);
    const Link sta = parse_link(sta_line);
    const bool down = down_trained(cap, sta);
    std::cout << name << "\n  " << cap_line << "\n  " << sta_line << "\n";
    std::printf("  capable    %4.1f GT/s x%-2d = %6.2f GB/s per direction\n", cap.gt_s, cap.width, link_gb_s(cap));
    std::printf("  negotiated %4.1f GT/s x%-2d = %6.2f GB/s per direction\n", sta.gt_s, sta.width, link_gb_s(sta));
    if (down && link_gb_s(cap) > 0.0)
        std::printf("  DOWN TRAINED: keeps %.0f%% of the bandwidth, loses %.2f GB/s\n\n",
                    100.0 * link_gb_s(sta) / link_gb_s(cap), link_gb_s(cap) - link_gb_s(sta));
    else
        std::printf("  ok: the link runs at its full speed and width\n\n");
    std::fflush(stdout);
    return down;
}

int main(int argc, char** argv) {
    try {
        if (argc == 3) {                     // the two lines of a real device
            report("your device", argv[1], argv[2]);
            return 0;
        }
        const bool a = report("sample 1: the card of slide 5, a x16 slot wired for 4 lanes on a Gen 3 port",
                              "LnkCap: Port #0, Speed 16GT/s, Width x16, ASPM L1",
                              "LnkSta: Speed 8GT/s (downgraded), Width x4 (downgraded)");
        const bool b = report("sample 2: an NVMe drive at full speed and width",
                              "LnkCap: Port #0, Speed 16GT/s, Width x4, ASPM L1",
                              "LnkSta: Speed 16GT/s (ok), Width x4 (ok)");
        const bool c = report("sample 3: an idle graphics card, full width but 2.5GT/s to save power: check under load",
                              "LnkCap: Port #0, Speed 16GT/s, Width x16, ASPM L0s L1",
                              "LnkSta: Speed 2.5GT/s (downgraded), Width x16");
        const bool ok = a && !b && c;        // correctness check: samples 1 and 3 flagged, sample 2 not
        std::cout << (ok ? "check: ok\n" : "check: FAILED\n");
        return ok ? 0 : 1;
    } catch (const std::exception& e) {
        std::cout << "could not parse a number after Speed or Width: " << e.what() << "\n";
        return 1;
    }
}
