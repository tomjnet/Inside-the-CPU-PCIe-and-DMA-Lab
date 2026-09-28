/* PCIe Enumeration and Configuration Space in 7 Minutes - slide 4: the configuration header (C version of 04_class_code.cpp) */
/* Build: make 04_class_code_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stddef.h>
#include <stdio.h>

/* class code, 24 bits at offset 0x09: base class, subclass, prog IF */
typedef struct { unsigned base; const char *name; } ClassName;
static const ClassName kClasses[] = {
    {0x01, "mass storage"},   /* 0x010802: NVMe */
    {0x02, "network"},        /* 0x020000: Ethernet */
    {0x03, "display"},        /* 0x030000: VGA, 0x030200: 3D */
    {0x06, "bridge"},         /* 0x060400: PCI to PCI, a root port */
    {0x0c, "serial bus"}};    /* 0x0c0330: USB xHCI */
static unsigned base_of(unsigned cc) { return cc >> 16; }   /* no device access */

/* The rest of the split, and the lookup the table is for. */
static unsigned subclass_of(unsigned cc) { return (cc >> 8) & 0xffu; }
static unsigned prog_if_of(unsigned cc) { return cc & 0xffu; }

static const char *class_name(unsigned cc) {
    for (size_t i = 0; i < sizeof kClasses / sizeof kClasses[0]; ++i)
        if (kClasses[i].base == base_of(cc)) return kClasses[i].name;
    return "other";
}

/* A few well known full codes, so the output names the device and not only its family. */
static const char *full_name(unsigned cc) {
    switch (cc) {
        case 0x010802: return "NVMe controller";
        case 0x010601: return "SATA controller (AHCI)";
        case 0x020000: return "Ethernet controller";
        case 0x028000: return "other network controller (many Wi-Fi cards)";
        case 0x030000: return "VGA compatible controller";
        case 0x030200: return "3D controller (a GPU with no display output)";
        case 0x060000: return "host bridge";
        case 0x060400: return "PCI to PCI bridge (root ports and switch ports)";
        case 0x0c0330: return "USB xHCI controller";
        default:       return "(not in this small table)";
    }
}

int main(void) {
    const unsigned samples[] = {0x010802, 0x010601, 0x020000, 0x028000, 0x030000,
                                0x030200, 0x060000, 0x060400, 0x0c0330, 0x088000};

    printf("class code   base  sub  prog IF   family         device\n");
    int wrong = 0;
    for (size_t i = 0; i < sizeof samples / sizeof samples[0]; ++i) {
        const unsigned cc = samples[i];
        printf("0x%06x     0x%02x  0x%02x 0x%02x      %-15s%s\n", cc, base_of(cc), subclass_of(cc),
               prog_if_of(cc), class_name(cc), full_name(cc));
        const unsigned rebuilt = (base_of(cc) << 16) | (subclass_of(cc) << 8) | prog_if_of(cc);
        if (rebuilt != cc) ++wrong;
    }

    printf("\nthe class code tells a generic driver what the device is: the NVMe driver binds to every\n"
           "0x010802, whoever made it; vendor ID and device ID are for drivers of one specific chip\n");
    if (wrong != 0) {
        printf("FAILED: %d codes did not survive the split\n", wrong);
        return 1;
    }
    return 0;
}
