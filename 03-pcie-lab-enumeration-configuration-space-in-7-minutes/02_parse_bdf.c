/* PCIe Enumeration and Configuration Space in 7 Minutes - slide 2: an address for every function: bdf (C version of 02_parse_bdf.cpp) */
/* Build: make 02_parse_bdf_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* Helper the slide assumes: n hex digits of s, starting at `at`, as a number. The caller checked the digits. */
static unsigned hex(const char *s, size_t at, size_t n) {
    unsigned value = 0;
    for (size_t i = at; i < at + n; ++i) {
        const char c = s[i];
        const unsigned digit = (c >= '0' && c <= '9') ? (unsigned)(c - '0')
                                                      : (unsigned)(tolower((unsigned char)c) - 'a') + 10u;
        value = value * 16u + digit;
    }
    return value;
}

/* Added for the sample: every position that is not a separator must be a hex digit. */
static bool all_hex(const char *s, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (i == 4 || i == 7 || i == 10) continue;
        if (isxdigit((unsigned char)s[i]) == 0) return false;
    }
    return true;
}

/* "0000:03:00.0" = domain:bus:device.function, all hex */
typedef struct { unsigned domain, bus, device, function; } Bdf;

/* C has no std::optional: return true and fill *out, or return false */
static bool parse_bdf(const char *s, Bdf *out) {
    const size_t len = strlen(s);
    if (len != 12 || s[4] != ':' || s[7] != ':' || s[10] != '.')
        return false;                     /* fixed shape: 12 characters */
    if (!all_hex(s, len)) return false;   /* sample only: the slide assumes hex digits */
    Bdf b = {hex(s, 0, 4), hex(s, 5, 2), hex(s, 8, 2), hex(s, 11, 1)};
    if (b.device > 31 || b.function > 7) return false;
    *out = b;                             /* 16 + 8 + 5 + 3 bits */
    return true;
}

int main(void) {
    typedef struct { const char *text; bool valid; } Case;
    const Case cases[] = {
        {"0000:03:00.0", true},    /* the NIC of the slides */
        {"0000:01:00.1", true},    /* function 1: the audio controller of a GPU */
        {"0000:00:1c.0", true},    /* device 0x1c = 28 on bus 0: a root port */
        {"03:00.0", false},        /* the short form lspci prints: add the domain first */
        {"0000:03:20.0", false},   /* device 0x20 = 32: only 5 bits, 0 to 31 */
        {"0000:03:00.8", false},   /* function 8: only 3 bits, 0 to 7 */
        {"0000:zz:00.0", false},   /* not hex */
    };
    const size_t n_cases = sizeof cases / sizeof cases[0];

    int wrong = 0;
    for (size_t i = 0; i < n_cases; ++i) {
        const Case *c = &cases[i];
        Bdf b;
        const bool ok = parse_bdf(c->text, &b);
        printf("%s  ->  ", c->text);
        if (ok) {
            /* bus, device and function packed the way the hardware routes them: 8 + 5 + 3 = 16 bits. */
            const unsigned routing_id = (b.bus << 8) | (b.device << 3) | b.function;
            /* Where the 4096 bytes of this function sit inside the memory mapped configuration window (ECAM). */
            const unsigned long long ecam = ((unsigned long long)b.bus << 20)
                                            | ((unsigned long long)b.device << 15)
                                            | ((unsigned long long)b.function << 12);
            printf("domain %u, bus %u, device %u, function %u   routing ID 0x%x, ECAM offset 0x%llx\n",
                   b.domain, b.bus, b.device, b.function, routing_id, ecam);
        } else {
            printf("rejected\n");
        }
        if (ok != c->valid) ++wrong;
    }

    printf("\nlimits: 65536 domains, 256 buses per domain, 32 devices per bus, 8 functions per device\n");
    printf("on PCIe a link has one device at the far end, so the device field is almost always 00\n");
    if (wrong != 0) {
        printf("FAILED: %d addresses were classified wrongly\n", wrong);
        return 1;
    }
    printf("all %zu addresses classified as expected\n", n_cases);
    return 0;
}
