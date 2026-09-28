/* NUMA Aware PCIe in 9 Minutes: NICs, NVMe and GPUs - slide 8: parse local_cpulist (C version of 08_parse_cpulist.cpp) */
/* Build: make 08_parse_cpulist_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__)
#include <dirent.h>
#endif

/* a growable int list: C has no std::vector, so the caller frees v */
typedef struct { int *v; size_t n; size_t cap; } IntList;

static int push(IntList *l, int x) {
    if (l->n == l->cap) {
        size_t cap = l->cap == 0 ? 16 : l->cap * 2;
        int *p = realloc(l->v, cap * sizeof *p);
        if (p == NULL) return -1;
        l->v = p;
        l->cap = cap;
    }
    l->v[l->n++] = x;
    return 0;
}

/* read one number at *s, advance past it; -1 when there is none (C has no stoi exception) */
static int read_int(const char **s, int *out) {
    char *end;
    long x = strtol(*s, &end, 10);
    if (end == *s || x < 0 || x > 1000000) return -1;
    *out = (int)x;
    *s = end;
    return 0;
}

/* "0-15,32-47" to 0..15 and 32..47: the format of local_cpulist.
   Returns 0, or -1 on text that is not a cpulist (the C++ twin throws). */
static int parse_cpulist(const char *text, IntList *cpus) {
    const char *s = text;
    cpus->n = 0;
    while (*s != '\0') {
        int lo, hi;
        if (read_int(&s, &lo) != 0) return -1;
        hi = lo;
        if (*s == '-') {                          /* no dash: a single CPU */
            ++s;
            if (read_int(&s, &hi) != 0) return -1;
        }
        for (int c = lo; c <= hi; ++c)
            if (push(cpus, c) != 0) return -1;
        if (*s == ',') ++s;
        else if (*s != '\0') return -1;
    }
    return 0;                                     /* pin the workers here */
}

static void show(const char *text) {
    IntList cpus = {NULL, 0, 0};
    if (parse_cpulist(text, &cpus) != 0) {
        printf("  could not parse \"%s\": not a cpulist\n", text);
        free(cpus.v);
        return;
    }
    printf("  \"%s\" is %zu CPUs:", text, cpus.n);
    const size_t head = cpus.n > 8 ? 6 : cpus.n;  /* keep long lists on one line */
    for (size_t i = 0; i < head; ++i) printf(" %d", cpus.v[i]);
    if (head < cpus.n) {
        printf(" (and %zu more) %d", cpus.n - head - 1, cpus.v[cpus.n - 1]);
    }
    printf("\n");
    free(cpus.v);
}

int main(void) {
    printf("sample strings (typical local_cpulist contents):\n");
    show("0-15,32-47");                           /* physical cores, then their hyperthread siblings */
    show("16-31,48-63");
    show("0-7");
    show("3");
    show("0,2,4-6");

    int failed = 0;
    const int expect[] = {0, 2, 4, 5, 6};
    IntList l = {NULL, 0, 0};
    if (parse_cpulist("0,2,4-6", &l) != 0 || l.n != 5 || memcmp(l.v, expect, sizeof expect) != 0) ++failed;
    if (parse_cpulist("0-15,32-47", &l) != 0 || l.n != 32) ++failed;
    if (parse_cpulist("", &l) != 0 || l.n != 0) ++failed;
    free(l.v);
    printf(failed == 0 ? "self check: ok\n" : "self check: FAILED\n");

    /* the real file of the first PCIe device, when this machine has one */
    int found = 0;
#if defined(__linux__)
    const char *root = "/sys/bus/pci/devices";
    DIR *d = opendir(root);
    if (d != NULL) {
        struct dirent *e;
        while (!found && (e = readdir(d)) != NULL) {
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
            char path[512], line[1024];
            snprintf(path, sizeof path, "%s/%s/local_cpulist", root, e->d_name);
            FILE *f = fopen(path, "r");
            if (f == NULL) continue;
            if (fgets(line, sizeof line, f) != NULL) {
                line[strcspn(line, "\n")] = '\0';
                if (line[0] != '\0') {
                    printf("this machine, %s:\n", e->d_name);
                    show(line);
                    found = 1;
                }
            }
            fclose(f);
        }
        closedir(d);
    }
#endif
    if (!found) printf("no local_cpulist file here (Windows, WSL or no PCIe devices): samples only\n");
    return failed == 0 ? 0 : 1;
}
