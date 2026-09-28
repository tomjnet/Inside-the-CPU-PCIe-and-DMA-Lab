/* PCIe Lanes, Generations and Link Width in 7 Minutes - slide 7: flag a down trained link in c++ (C version of 07_parse_lnksta.cpp) */
/* Build: make 07_parse_lnksta_c */
/* Parses the LnkCap and LnkSta lines of lspci -vv and reports the bandwidth kept and lost. */
/* Usage: ./07_parse_lnksta_c                                 sample lines (WSL shows no PCIe devices) */
/*        ./07_parse_lnksta_c "LnkCap line" "LnkSta line"     the two lines of a real device */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { double gt_s; int width; } Link;

/* "Speed 8GT/s, Width x4": one pass over one line.
   C has no exceptions, so a missing number after the key returns false instead of throwing. */
static bool parse_link(const char *line, Link *l) {
    l->gt_s = 0;
    l->width = 0;
    const char *s = strstr(line, "Speed "), *w = strstr(line, "Width x");
    if (s != NULL) {
        char *end = NULL;
        l->gt_s = strtod(s + 6, &end);
        if (end == s + 6) return false;
    }
    if (w != NULL) {
        char *end = NULL;
        l->width = (int)strtol(w + 7, &end, 10);
        if (end == w + 7) return false;
    }
    return true;
}
static bool down_trained(const Link *cap, const Link *sta) {
    return sta->gt_s < cap->gt_s || sta->width < cap->width;  /* slower link */
}

/* the calculator of slide 4: 8b/10b up to 5 GT/s (Gen 1 and 2), 128b/130b from 8 GT/s (Gen 3 to 5) */
static double link_gb_s(const Link *l) {
    const double encoding = l->gt_s <= 5.0 ? 8.0 / 10.0 : 128.0 / 130.0;
    return l->gt_s * encoding * l->width / 8.0;
}

/* returns 1 when down trained, 0 when not, -1 when a line has no number after Speed or Width */
static int report(const char *name, const char *cap_line, const char *sta_line) {
    Link cap, sta;
    if (!parse_link(cap_line, &cap) || !parse_link(sta_line, &sta)) return -1;
    const bool down = down_trained(&cap, &sta);
    printf("%s\n  %s\n  %s\n", name, cap_line, sta_line);
    printf("  capable    %4.1f GT/s x%-2d = %6.2f GB/s per direction\n", cap.gt_s, cap.width, link_gb_s(&cap));
    printf("  negotiated %4.1f GT/s x%-2d = %6.2f GB/s per direction\n", sta.gt_s, sta.width, link_gb_s(&sta));
    if (down && link_gb_s(&cap) > 0.0)
        printf("  DOWN TRAINED: keeps %.0f%% of the bandwidth, loses %.2f GB/s\n\n",
               100.0 * link_gb_s(&sta) / link_gb_s(&cap), link_gb_s(&cap) - link_gb_s(&sta));
    else
        printf("  ok: the link runs at its full speed and width\n\n");
    fflush(stdout);
    return down ? 1 : 0;
}

static int parse_failed(void) {
    printf("could not parse a number after Speed or Width\n");
    return 1;
}

int main(int argc, char **argv) {
    if (argc == 3) {                         /* the two lines of a real device */
        return report("your device", argv[1], argv[2]) < 0 ? parse_failed() : 0;
    }
    const int a = report("sample 1: the card of slide 5, a x16 slot wired for 4 lanes on a Gen 3 port",
                         "LnkCap: Port #0, Speed 16GT/s, Width x16, ASPM L1",
                         "LnkSta: Speed 8GT/s (downgraded), Width x4 (downgraded)");
    if (a < 0) return parse_failed();
    const int b = report("sample 2: an NVMe drive at full speed and width",
                         "LnkCap: Port #0, Speed 16GT/s, Width x4, ASPM L1",
                         "LnkSta: Speed 16GT/s (ok), Width x4 (ok)");
    if (b < 0) return parse_failed();
    const int c = report("sample 3: an idle graphics card, full width but 2.5GT/s to save power: check under load",
                         "LnkCap: Port #0, Speed 16GT/s, Width x16, ASPM L0s L1",
                         "LnkSta: Speed 2.5GT/s (downgraded), Width x16");
    if (c < 0) return parse_failed();
    const bool ok = a == 1 && b == 0 && c == 1;  /* correctness check: samples 1 and 3 flagged, sample 2 not */
    fputs(ok ? "check: ok\n" : "check: FAILED\n", stdout);
    return ok ? 0 : 1;
}
