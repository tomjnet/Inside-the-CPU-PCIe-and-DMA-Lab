/* PCIe Enumeration and Configuration Space in 7 Minutes - slide 6: read the identity from sysfs (C version of 06_sysfs_reader.cpp) */
/* Build: make 06_sysfs_reader_c */
/*
 * C has no std::filesystem: opendir, readdir and fopen inside __linux__. On Windows, and on a Linux with no
 * PCIe devices in sysfs (WSL shows few or none), the program says so and exits 0.
 */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stddef.h>
#include <stdio.h>

#if defined(__linux__)
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* Helper the slide assumes: a sysfs identity file holds one number such as 0x8086. 0 when it cannot be read. */
static unsigned read_hex(const char *dir, const char *name) {
    char path[512];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *in = fopen(path, "r");
    if (in == NULL) return 0;
    char text[64];
    unsigned value = 0;
    if (fgets(text, sizeof text, in) != NULL) {
        char *end = NULL;
        const unsigned long v = strtoul(text, &end, 16);   /* no exceptions in C: check end instead */
        if (end != text) value = (unsigned)v;
    }
    fclose(in);
    return value;
}

/* The decoder of slide 4: the top byte of the class code picks the family. */
typedef struct { unsigned base; const char *name; } ClassName;
static const ClassName kClasses[] = {
    {0x01, "mass storage"}, {0x02, "network"}, {0x03, "display"}, {0x04, "multimedia"},
    {0x05, "memory"},       {0x06, "bridge"},  {0x08, "system peripheral"}, {0x0c, "serial bus"}};

static const char *class_name(unsigned cc) {
    for (size_t i = 0; i < sizeof kClasses / sizeof kClasses[0]; ++i)
        if (kClasses[i].base == (cc >> 16)) return kClasses[i].name;
    return "other";
}
#endif

int main(void) {
    const char *root = "/sys/bus/pci/devices";
    int rows = 0;

#if defined(__linux__)
    DIR *d = opendir(root);
    if (d != NULL) {
        printf("BDF           vendor:device  class\n");
        /* one directory per function: /sys/bus/pci/devices/0000:03:00.0 */
        /* the kernel filled these files at enumeration: no PCIe crossing */
        errno = 0;
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (e->d_name[0] == '.') continue;          /* skip . and .. */
            char dir[300];
            snprintf(dir, sizeof dir, "%s/%s", root, e->d_name);
            unsigned vendor = read_hex(dir, "vendor");  /* 0x8086 */
            unsigned device = read_hex(dir, "device");  /* 0x1533 */
            unsigned cc     = read_hex(dir, "class");   /* 0x020000 */
            printf("%s  %x:%x  %s\n", e->d_name, vendor, device, class_name(cc));
            ++rows;
            errno = 0;
        }
        if (errno != 0) printf("stopped reading %s: %s\n", root, strerror(errno));
        closedir(d);   /* C has no destructor: close the directory by hand */
        /* missing or empty directory: no PCIe devices visible here (WSL) */
    }
#endif

    if (rows == 0) {
        printf("no PCIe devices visible here (WSL, a container or not Linux): %s is missing or empty\n", root);
        printf("on real Linux hardware this prints one row per function, for example:\n");
        printf("  0000:03:00.0  8086:1533  network\n");
        printf("  0000:04:00.0  144d:a808  mass storage\n");
    } else {
        printf("%d functions, read from files the kernel cached: no root, no driver, no PCIe crossing\n", rows);
    }
    return 0;
}
