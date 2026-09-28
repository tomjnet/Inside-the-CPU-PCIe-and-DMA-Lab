/* Build an End to End Low Latency I/O Map in 10 Minutes - slide 11: then measure: the median and the tail (C version of 11_percentiles.cpp) */
/* Build: make 11_percentiles_c */
/*
 * Times every request, keeps every sample and reads p50, p99 and p99.9 by rank, before and after one change.
 * The request is a stand in for your I/O path: a 4 KiB read of a temporary file. Before: fopen, fread and
 * fclose on every request. After: the file stays open, fseek and fread. The numbers belong to this machine
 * only and the program never asserts on them. With 4000 samples p99.9 rests on the 4 slowest requests: on a
 * real audit run the real load for minutes, on real Linux hardware.
 */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- timing: now_ns, sink and cmp_double of the canonical C harness ---- */
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static double now_ns(void) {
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1e9 / (double)f.QuadPart;
}
#else
#include <time.h>
static double now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}
#endif

static volatile uint64_t g_sink = 0;
static void sink(uint64_t x) { g_sink = g_sink + x; }   /* keeps the result alive: the loop cannot be deleted */

static int cmp_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

#define BLOCK 4096
#define SAMPLES 4000

typedef struct {
    const char *path;                /* the temporary file */
    FILE *kept;                      /* the "after" path: opened once */
    unsigned char buf[BLOCK];
} Ctx;

static uint64_t checksum(const unsigned char *buf, size_t got) {
    uint64_t sum = 0;
    for (size_t i = 0; i < got; ++i) sum += buf[i];
    return sum;
}

/* before: open, read 4 KiB, close on every request */
static uint64_t open_read_close(void *p) {
    Ctx *c = (Ctx *)p;
    FILE *in = fopen(c->path, "rb");
    if (in == NULL) return 0;
    size_t got = fread(c->buf, 1, sizeof c->buf, in);
    fclose(in);
    return checksum(c->buf, got);
}

/* after: the file stays open, seek and read */
static uint64_t seek_read(void *p) {
    Ctx *c = (Ctx *)p;
    if (c->kept == NULL) return 0;
    fseek(c->kept, 0, SEEK_SET);
    size_t got = fread(c->buf, 1, sizeof c->buf, c->kept);
    return checksum(c->buf, got);
}

/* a function pointer plus a context pointer: the C form of the C++ template and lambda */
static void measure(const char *label, uint64_t (*one_request)(void *), void *ctx, double *ns) {
    const int n = SAMPLES;
    for (int i = 0; i < 200; ++i) sink(one_request(ctx));      /* warm up: page cache, allocator, branch predictor */

    /* time every request, keep every sample: the tail is the finding */
    for (int i = 0; i < n; ++i) {
        double t0 = now_ns();
        sink(one_request(ctx));                /* the path under test */
        ns[i] = now_ns() - t0;
    }
    qsort(ns, (size_t)n, sizeof ns[0], cmp_double);   /* rank = percentile */
    double p50 = ns[(size_t)(0.50 * (n - 1))];
    double p99 = ns[(size_t)(0.99 * (n - 1))];
    double p999 = ns[(size_t)(0.999 * (n - 1))];
    printf("  %-36s%10.0f%10.0f%10.0f%11.1fx\n", label, p50, p99, p999,   /* before, then after */
           p50 > 0.0 ? p999 / p50 : 0.0);
}

int main(void) {
    char path[1024];
#if defined(_WIN32)
    const char *dir = getenv("TEMP");
#elif defined(__linux__)
    const char *dir = "/tmp";
#else
    const char *dir = NULL;
#endif
    if (dir == NULL || dir[0] == '\0') dir = ".";
    snprintf(path, sizeof path, "%s/tomjnet_pcie_lab_09_percentiles.bin", dir);

    FILE *out = fopen(path, "wb");
    if (out == NULL) {
        printf("cannot create %s: %s (the requests read nothing)\n", path, strerror(errno));
    } else {
        unsigned char block[BLOCK];
        memset(block, 'x', sizeof block);
        fwrite(block, 1, sizeof block, out);
        fclose(out);
    }

    /* C has no std::vector: one malloc for the samples, one free at the end */
    double *ns = malloc(SAMPLES * sizeof *ns);
    static Ctx ctx;
    if (ns == NULL) {
        printf("out of memory\n");
        remove(path);
        return 1;
    }
    ctx.path = path;

    printf("4000 requests each, nanoseconds, this machine        p50       p99     p99.9  p99.9/p50\n");

    measure("before: open, read 4 KiB, close", open_read_close, &ctx, ns);

    ctx.kept = fopen(path, "rb");
    measure("after: file kept open, seek, read", seek_read, &ctx, ns);
    if (ctx.kept != NULL) fclose(ctx.kept);   /* C has no destructor: close by hand */

    free(ns);
    remove(path);
    printf("the median describes the path (fewer system calls after the change), the tail describes the\n"
           "collisions (the scheduler, an interrupt, another process): compare both, one change at a time\n");
    return 0;
}
