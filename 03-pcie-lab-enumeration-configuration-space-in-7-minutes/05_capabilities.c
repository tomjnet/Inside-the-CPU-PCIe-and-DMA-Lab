/* PCIe Enumeration and Configuration Space in 7 Minutes - slide 5: capabilities: a linked list inside the space (C version of 05_capabilities.cpp) */
/* Build: make 05_capabilities_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* C has no std::array: a struct around the 256 bytes copies by value just the same */
typedef struct { uint8_t b[256]; } Config;

/* Helper the slide assumes: the names of the capability IDs a viewer meets first. */
static const char *cap_name(unsigned id) {
    switch (id) {
        case 0x01: return "power management";
        case 0x05: return "MSI";
        case 0x10: return "PCI Express (LnkCap and LnkSta live here)";
        case 0x11: return "MSI-X";
        default:   return "(another capability)";
    }
}

/* cfg: the 256 bytes of one function. Status bit 4: list present */
static void walk_capabilities(const Config *cfg) {
    if ((cfg->b[0x06] & 0x10) == 0) return;       /* no capability list */
    for (unsigned p = cfg->b[0x34] & 0xfcu; p; p = cfg->b[p + 1] & 0xfcu) {
        unsigned id = cfg->b[p];                   /* byte 0: ID, byte 1: next */
        printf("cap 0x%x at 0x%x  %s\n", id, p, cap_name(id));  /* 0x10: PCIe */
    }                                              /* next == 0 ends the list */
}

/* The same walk as a counter, with the guard a real tool adds: a broken device must not loop forever. */
static int count_capabilities(const Config *cfg) {
    if ((cfg->b[0x06] & 0x10) == 0) return 0;
    int count = 0;
    for (unsigned p = cfg->b[0x34] & 0xfcu; p != 0 && count < 48; p = cfg->b[p + 1] & 0xfcu) ++count;
    return count;
}

/* Configuration space is little endian: the low byte sits at the lower offset. */
static void put16(Config *cfg, size_t at, unsigned value) {
    cfg->b[at] = (uint8_t)(value & 0xffu);
    cfg->b[at + 1] = (uint8_t)((value >> 8) & 0xffu);
}
static unsigned get16(const Config *cfg, size_t at) {
    return (unsigned)cfg->b[at] | ((unsigned)cfg->b[at + 1] << 8);
}
static void put_cap(Config *cfg, size_t at, unsigned id, unsigned next) {
    cfg->b[at] = (uint8_t)id;
    cfg->b[at + 1] = (uint8_t)next;
}

/* A model of the NIC of the slides: the numbers are typical, nothing here reads a real device. */
static Config model_nic(void) {
    Config cfg = {{0}};
    put16(&cfg, 0x00, 0x8086);        /* vendor ID: Intel */
    put16(&cfg, 0x02, 0x1533);        /* device ID: I210 */
    put16(&cfg, 0x06, 0x0010);        /* status: bit 4 set, the capability list exists */
    cfg.b[0x09] = 0x00;               /* class code 0x020000, low byte first: prog IF */
    cfg.b[0x0a] = 0x00;               /*   subclass: Ethernet */
    cfg.b[0x0b] = 0x02;               /*   base class: network */
    cfg.b[0x0e] = 0x00;               /* header type 0: an endpoint, one function */
    cfg.b[0x34] = 0x40;               /* capabilities pointer */
    put_cap(&cfg, 0x40, 0x01, 0x50);  /* power management */
    put_cap(&cfg, 0x50, 0x05, 0x70);  /* MSI */
    put_cap(&cfg, 0x70, 0x10, 0xb0);  /* PCI Express */
    put_cap(&cfg, 0xb0, 0x11, 0x00);  /* MSI-X, next == 0: end of the list */
    return cfg;
}

int main(void) {
    const Config nic = model_nic();
    const unsigned cc = ((unsigned)nic.b[0x0b] << 16) | ((unsigned)nic.b[0x0a] << 8) | nic.b[0x09];

    printf("model of one function (typical values, not read from hardware)\n");
    printf("vendor 0x%x, device 0x%x, class 0x%x, header type 0x%x, cap ptr 0x%x\n\n", get16(&nic, 0x00),
           get16(&nic, 0x02), cc, (unsigned)nic.b[0x0e], (unsigned)nic.b[0x34]);

    walk_capabilities(&nic);
    const int found = count_capabilities(&nic);
    printf("%d capabilities in the list\n\n", found);

    Config legacy = nic;               /* struct assignment copies all 256 bytes */
    put16(&legacy, 0x06, 0x0000);      /* status bit 4 clear: the pointer at 0x34 must be ignored */
    printf("same bytes with status bit 4 cleared:\n");
    walk_capabilities(&legacy);
    const int none = count_capabilities(&legacy);
    printf("%d capabilities: the walk never reads 0x34\n", none);

    if (found != 4 || none != 0) {
        printf("FAILED: expected 4 and 0 capabilities\n");
        return 1;
    }
    return 0;
}
