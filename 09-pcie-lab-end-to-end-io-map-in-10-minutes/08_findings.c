/* Build an End to End Low Latency I/O Map in 10 Minutes - slide 8: from facts to findings (C version of 08_findings.cpp) */
/* Build: make 08_findings_c */
/*
 * A model: the audit of the slide runs on two built in maps, a healthy NIC and an NVMe drive with every
 * fact wrong, so each finding fires once. Fill an IoMap from your own machine with 07_io_map and the
 * audit scripts, then call audit() on it. Deterministic output, no Linux calls.
 */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdio.h>
#include <string.h>

#define MAX_VECTORS 64
#define MAX_CPUS 1024

typedef struct {                     /* one device, one row */
    const char *bdf, *speed_max, *speed_now;   /* "16.0 GT/s PCIe" */
    int width_max, width_now;        /* x8 capable, x8 trained? */
    int numa_node;                   /* -1: not reported */
    const char *iommu;               /* DMA, identity or none */
    int queues, ring;                /* vectors, descriptors */
    int irq_cpus[MAX_VECTORS];       /* CPU of each vector */
    int n_irq;
} IoMap;

/* C has no std::set: one flag per CPU number plus a count of the flags set */
typedef struct { unsigned char has[MAX_CPUS]; int size; } CpuSet;

static int cpuset_count(const CpuSet *s, int cpu) { return cpu >= 0 && cpu < MAX_CPUS && s->has[cpu]; }

static int findings = 0;
static void finding(const char *layer, const char *what) {
    ++findings;
    printf("    [%s] %s\n", layer, what);
}

/* "0-15,32-47" -> {0..15, 32..47}: the format of local_cpulist and smp_affinity_list */
static void parse_cpulist(const char *list, CpuSet *cpus) {
    size_t i = 0, n = strlen(list);
    memset(cpus, 0, sizeof *cpus);
    while (i < n) {
        int first = 0, last = 0;
        int range = 0, digits = 0;
        for (; i < n && list[i] != ','; ++i) {
            if (list[i] == '-') { range = 1; continue; }
            if (list[i] < '0' || list[i] > '9') continue;
            digits = 1;
            if (range) last = last * 10 + (list[i] - '0');
            else first = first * 10 + (list[i] - '0');
        }
        ++i;                                        /* skip the comma */
        if (!digits) continue;
        if (!range) last = first;
        for (int cpu = first; cpu <= last && cpu < MAX_CPUS; ++cpu) {
            if (!cpus->has[cpu]) { cpus->has[cpu] = 1; ++cpus->size; }
        }
    }
}

/* one finding per layer, in the order you fix them */
static void audit(const IoMap *m, int app_node, const CpuSet *local) {
    if (m->width_now < m->width_max || strcmp(m->speed_now, m->speed_max) != 0)
        finding("link", "down trained: check slot, riser, BIOS");
    if (m->numa_node >= 0 && m->numa_node != app_node)
        finding("placement", "remote DMA: move threads or card");
    for (int i = 0; i < m->n_irq; ++i)            /* one vector per queue */
        if (!cpuset_count(local, m->irq_cpus[i])) finding("irq", "vector on remote CPU");
    if (m->queues > local->size)
        finding("queues", "more queues than local cores");
    if (strcmp(m->iommu, "DMA") == 0) finding("iommu", "translated: map cost");
}

static int run(const char *title, const IoMap *m, int app_node, const char *local_cpulist) {
    static CpuSet local;
    parse_cpulist(local_cpulist, &local);
    printf("%s\n  %s  x%d of x%d, node %d, iommu %s, %d queues, ring %d\n", title, m->bdf, m->width_now,
           m->width_max, m->numa_node, m->iommu, m->queues, m->ring);
    printf("  app on node %d, local CPUs %s (%d cores)\n", app_node, local_cpulist, local.size);
    const int before = findings;
    audit(m, app_node, &local);
    const int fired = findings - before;
    if (fired == 0) printf("    no findings: the facts are right, now measure\n");
    return fired;
}

int main(void) {
    const IoMap nic = {
        "0000:3b:00.0", "16.0 GT/s PCIe", "16.0 GT/s PCIe",
        8, 8, 0, "identity", 4, 1024,
        {0, 1, 2, 3}, 4,
    };

    const IoMap nvme = {                 /* every layer wrong, on purpose */
        "0000:af:00.0", "16.0 GT/s PCIe", "8.0 GT/s PCIe",
        4, 2, 1, "DMA", 6, 1024,
        {0, 1, 2, 3, 16, 17}, 6,
    };

    const int healthy = run("healthy NIC", &nic, 0, "0-15,32-47");
    const int broken = run("broken NVMe drive (app pinned to four cores of node 0)", &nvme, 0, "0-3");

    printf("findings: healthy %d, broken %d\n", healthy, broken);
    printf("fix order: link, placement, irq, queues, then decide about the IOMMU\n");
    /* correctness: the healthy map is clean; the broken one fires link, placement, 2 irq, queues and iommu */
    if (healthy != 0 || broken != 6) {
        printf("unexpected number of findings\n");
        return 1;
    }
    return 0;
}
