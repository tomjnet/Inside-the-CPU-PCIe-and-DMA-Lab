/* Build an End to End Low Latency I/O Map in 10 Minutes - slide 7: the map as a program (C version of 07_io_map.cpp) */
/* Build: make 07_io_map_c */
/*
 * Prints one I/O map row per PCIe device from sysfs: link (capable against trained), NUMA node, IOMMU mode,
 * MSI or MSI-X vectors and the CPU each vector may run on. sysfs is plain text read with fopen and fgets;
 * the folder walk (opendir, readdir) is Linux only. Where /sys/bus/pci/devices does not exist or is empty
 * (Windows, WSL) it says so and prints built in sample data instead. Ring sizes are not in sysfs: read them
 * with ethtool -g (see 06_audit_queues_irqs_rings.sh).
 */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__)
#include <dirent.h>
#endif

#define MAX_VECTORS 256              /* C has no std::vector: fixed room for the vectors of one device */
#define MAX_DEVICES 1024
#define SHOWN 8

typedef struct {                     /* one device, one row */
    char bdf[256], speed_max[64], speed_now[64];  /* "0000:3b:00.0", "16.0 GT/s PCIe" */
    int width_max, width_now;        /* x8 capable, x8 trained? */
    int numa_node;                   /* -1: not reported */
    char iommu[64];                  /* DMA, identity or none */
    int queues, ring;                /* vectors, descriptors */
    int irq_cpus[MAX_VECTORS];       /* CPU of each vector */
    int n_irq;
} IoMap;

static void io_map_init(IoMap *m) {
    memset(m, 0, sizeof *m);
    m->numa_node = -1;
    snprintf(m->iommu, sizeof m->iommu, "none");
}

#if defined(__linux__)
static int cmp_int(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

/* first line of /sys/bus/pci/devices/<dev>/<f>, "" if absent: a text file read, zero PCIe crossings */
static void read_sysfs(const char *dev, const char *f, char *out, size_t cap) {
    char path[512];
    snprintf(path, sizeof path, "/sys/bus/pci/devices/%s/%s", dev, f);
    out[0] = '\0';
    FILE *in = fopen(path, "r");
    if (in == NULL) return;
    if (fgets(out, (int)cap, in) == NULL) out[0] = '\0';
    out[strcspn(out, "\n")] = '\0';
    fclose(in);
}

/* "8" -> 8, "" or text -> fallback (sysfs values are short decimal numbers, -1 for an unknown NUMA node) */
static int to_int(const char *s, int fallback) {
    size_t i = 0;
    int negative = 0;
    if (s[0] == '\0') return fallback;
    if (s[0] == '-') { negative = 1; i = 1; }
    if (s[i] < '0' || s[i] > '9') return fallback;
    int value = 0;
    for (; s[i] >= '0' && s[i] <= '9'; ++i) value = value * 10 + (s[i] - '0');
    return negative ? -value : value;
}

/* the first CPU of a list such as "0-15,32-47" or "3": enough to say where a vector lands */
static int first_cpu(const char *list) { return to_int(list, -1); }

static void load(IoMap *m, const char *bdf) {
    char s[128], type[64];
    io_map_init(m);
    snprintf(m->bdf, sizeof m->bdf, "%s", bdf);
    read_sysfs(bdf, "max_link_speed", m->speed_max, sizeof m->speed_max);
    read_sysfs(bdf, "current_link_speed", m->speed_now, sizeof m->speed_now);
    read_sysfs(bdf, "max_link_width", s, sizeof s);
    m->width_max = to_int(s, 0);
    read_sysfs(bdf, "current_link_width", s, sizeof s);
    m->width_now = to_int(s, 0);
    read_sysfs(bdf, "numa_node", s, sizeof s);
    m->numa_node = to_int(s, -1);
    read_sysfs(bdf, "iommu_group/type", type, sizeof type);           /* DMA, DMA-FQ or identity */
    if (type[0] != '\0') snprintf(m->iommu, sizeof m->iommu, "%s", strncmp(type, "DMA", 3) == 0 ? "DMA" : type);

    /* one file per MSI or MSI-X vector, named by its IRQ number */
    char path[512];
    snprintf(path, sizeof path, "/sys/bus/pci/devices/%s/msi_irqs", bdf);
    DIR *d = opendir(path);
    if (d != NULL) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL && m->n_irq < MAX_VECTORS) {
            if (e->d_name[0] == '.') continue;
            char aff[512], cpus[256];
            snprintf(aff, sizeof aff, "/proc/irq/%s/smp_affinity_list", e->d_name);
            cpus[0] = '\0';
            FILE *f = fopen(aff, "r");
            if (f != NULL) {
                if (fgets(cpus, sizeof cpus, f) == NULL) cpus[0] = '\0';
                fclose(f);
            }
            m->irq_cpus[m->n_irq++] = first_cpu(cpus);
        }
        closedir(d);
    }
    qsort(m->irq_cpus, (size_t)m->n_irq, sizeof m->irq_cpus[0], cmp_int);
    m->queues = m->n_irq;
}

typedef struct { char name[sizeof ((struct dirent *)0)->d_name]; } Bdf;   /* room for any folder name */

static int cmp_bdf(const void *a, const void *b) {
    return strcmp(((const Bdf *)a)->name, ((const Bdf *)b)->name);
}
#endif

static int sample_data(IoMap *maps) {
    static const int nic_cpus[] = {0, 1, 2, 3};
    static const int nvme_cpus[] = {0, 1, 2, 3, 16, 17, 18, 19};
    IoMap *nic = &maps[0], *nvme = &maps[1];

    io_map_init(nic);
    snprintf(nic->bdf, sizeof nic->bdf, "0000:3b:00.0");
    snprintf(nic->speed_max, sizeof nic->speed_max, "16.0 GT/s PCIe");
    snprintf(nic->speed_now, sizeof nic->speed_now, "16.0 GT/s PCIe");
    nic->width_max = nic->width_now = 8;
    nic->numa_node = 0;
    snprintf(nic->iommu, sizeof nic->iommu, "identity");
    nic->queues = 4;
    nic->ring = 1024;
    memcpy(nic->irq_cpus, nic_cpus, sizeof nic_cpus);
    nic->n_irq = 4;

    io_map_init(nvme);                   /* trained at half its width, on the other socket */
    snprintf(nvme->bdf, sizeof nvme->bdf, "0000:af:00.0");
    snprintf(nvme->speed_max, sizeof nvme->speed_max, "16.0 GT/s PCIe");
    snprintf(nvme->speed_now, sizeof nvme->speed_now, "16.0 GT/s PCIe");
    nvme->width_max = 4;
    nvme->width_now = 2;
    nvme->numa_node = 1;
    snprintf(nvme->iommu, sizeof nvme->iommu, "DMA");
    nvme->queues = 8;
    nvme->ring = 1024;
    memcpy(nvme->irq_cpus, nvme_cpus, sizeof nvme_cpus);
    nvme->n_irq = 8;
    return 2;
}

static void print(const IoMap *m) {
    printf("  %s\n", m->bdf);
    printf("    1 link    x%d at %s   capable: x%d at %s", m->width_now, m->speed_now[0] ? m->speed_now : "?",
           m->width_max, m->speed_max[0] ? m->speed_max : "?");
    if (m->width_now < m->width_max || strcmp(m->speed_now, m->speed_max) != 0) printf("   [down trained]");
    printf("\n");
    printf("    2 NUMA    node %d%s\n", m->numa_node, m->numa_node < 0 ? " (not reported: single node)" : "");
    printf("    3 IOMMU   %s\n", m->iommu);
    printf("    4 queues  %d vectors\n", m->queues);
    printf("    5 IRQs    CPUs:");
    for (int i = 0; i < m->n_irq; ++i) printf(" %d", m->irq_cpus[i]);
    if (m->n_irq == 0) printf(" none (no MSI vectors, or the driver is not bound)");
    printf("\n");
    if (m->ring > 0) printf("    6 ring    %d descriptors\n", m->ring);
    else printf("    6 ring    not in sysfs: ethtool -g <nic>\n");
}

int main(void) {
    static IoMap maps[SHOWN];        /* about 1 KiB per row: static, not on the stack */
    int shown = 0;
    int found = 0;

#if defined(__linux__)
    static Bdf names[MAX_DEVICES];
    DIR *d = opendir("/sys/bus/pci/devices");
    if (d != NULL) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL && found < MAX_DEVICES) {
            if (e->d_name[0] == '.') continue;
            char s[64];
            read_sysfs(e->d_name, "max_link_width", s, sizeof s);
            if (s[0] == '\0') continue;                   /* not a PCIe link (host bridge, legacy) */
            if (to_int(s, 0) == 0) continue;
            snprintf(names[found].name, sizeof names[found].name, "%s", e->d_name);
            ++found;
        }
        closedir(d);
    }
    qsort(names, (size_t)found, sizeof names[0], cmp_bdf);
    for (int i = 0; i < found && i < SHOWN; ++i) load(&maps[shown++], names[i].name);
#endif

    if (found == 0) {
        printf("no PCIe devices visible here (Windows, WSL or a container): built in sample data\n");
        shown = sample_data(maps);
    } else {
        printf("I/O map from /sys/bus/pci/devices, %d PCIe devices (first 8 shown)\n", found);
    }
    for (int i = 0; i < shown; ++i) print(&maps[i]);
    printf("six facts per device: run 08_findings to turn them into findings\n");
    return 0;
}
