/* NUMA Aware PCIe in 9 Minutes: NICs, NVMe and GPUs - slide 7: read numa_node for every device (C version of 07_read_numa_node.cpp) */
/* Build: make 07_read_numa_node_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdio.h>
#include <string.h>

#if defined(__linux__)
#include <dirent.h>
#include <errno.h>
#endif

#define MAX_NODES 64

/* node number to device count, kept sorted by node: C has no std::map */
typedef struct { int node; int count; } NodeCount;

#if defined(__linux__)
/* first line of a small sysfs text file, or "?" when it is missing or unreadable */
static void read_line(const char *path, char *out, size_t cap) {
    FILE *f = fopen(path, "r");
    snprintf(out, cap, "?");
    if (f == NULL) return;
    char line[256];
    if (fgets(line, sizeof line, f) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] != '\0') snprintf(out, cap, "%s", line);
    }
    fclose(f);
}

/* ++per_node[node], keeping the array sorted like std::map */
static void add_node(NodeCount *per_node, int *used, int node) {
    int i = 0;
    while (i < *used && per_node[i].node < node) ++i;
    if (i < *used && per_node[i].node == node) { ++per_node[i].count; return; }
    if (*used == MAX_NODES) return;
    memmove(&per_node[i + 1], &per_node[i], (size_t)(*used - i) * sizeof per_node[0]);
    per_node[i].node = node;
    per_node[i].count = 1;
    ++*used;
}
#endif

int main(void) {
    int seen = 0;
    NodeCount per_node[MAX_NODES] = {{0, 0}};
    int used = 0;                                 /* distinct nodes in per_node */
    const char *root = "/sys/bus/pci/devices";
    const char *err = NULL;                       /* set when the folder itself is missing */

#if defined(__linux__)
    /* one folder per device, two small text files: no PCIe crossing at all */
    DIR *d = opendir(root);
    if (d == NULL) {
        err = strerror(errno);
    } else {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
            char path[512];
            int node = -1;                        /* -1: firmware gave no node */
            snprintf(path, sizeof path, "%s/%s/numa_node", root, e->d_name);
            FILE *f = fopen(path, "r");
            if (f != NULL) {
                if (fscanf(f, "%d", &node) != 1) node = -1;
                fclose(f);
            }
            char cpus[256];
            snprintf(path, sizeof path, "%s/%s/local_cpulist", root, e->d_name);
            read_line(path, cpus, sizeof cpus);
            printf("%s  node %d  cpus %s\n", e->d_name, node, cpus);
            ++seen;
            add_node(per_node, &used, node);
        }
        closedir(d);
    }
#else
    err = "not a Linux system";
#endif
    if (seen == 0) printf("no PCIe devices visible here (WSL)\n");

    if (err != NULL) {
        /* Windows, macOS, or a Linux without sysfs: the folder itself is missing */
        printf("this sample needs Linux: it would list every PCIe device with its NUMA node\n"
               "  (%s: %s)\n", root, err);
    }

    if (seen > 0) {
        printf("\ndevices per NUMA node:\n");
        for (int i = 0; i < used; ++i) {
            printf("  node %d: %d%s\n", per_node[i].node, per_node[i].count,
                   per_node[i].node < 0 ? "  (the firmware reported no node)" : "");
        }
        if (used <= 1) {
            printf("one node only: every path is local on this machine\n");
        } else {
            printf("several nodes: put buffers, IRQs and threads on the node of each device\n");
        }
    }
    printf("on a two socket server the list splits in two, for example:\n"
           "  0000:5e:00.0  node 0  cpus 0-15,32-47     (typical, not from this machine)\n"
           "  0000:af:00.0  node 1  cpus 16-31,48-63    (typical, not from this machine)\n");
    return 0;
}
