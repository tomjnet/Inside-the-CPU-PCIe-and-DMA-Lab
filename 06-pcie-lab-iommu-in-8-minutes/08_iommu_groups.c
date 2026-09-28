/* IOMMU in 8 Minutes: Translation and Isolation for Devices - slide 8: iommu groups and vfio (C version of 08_iommu_groups.cpp) */
/* Build: make 08_iommu_groups_c */
#if defined(__linux__)
#define _GNU_SOURCE
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/* C has no std::filesystem: a directory is listed with opendir on Linux and FindFirstFile on Windows */
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#define PATH_LEN 512

typedef void (*EntryFn)(const char *dir, const char *name, void *ctx);

static bool is_dot(const char *n) { return n[0] == '.' && (n[1] == '\0' || (n[1] == '.' && n[2] == '\0')); }

/* calls fn once per entry of dir (without . and ..); returns the count, or 0 when dir cannot be read */
static size_t for_each_entry(const char *dir, EntryFn fn, void *ctx) {
    size_t n = 0;
#if defined(_WIN32)
    char pattern[PATH_LEN];
    snprintf(pattern, sizeof pattern, "%s/*", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (is_dot(fd.cFileName)) continue;
        ++n;
        if (fn != NULL) fn(dir, fd.cFileName, ctx);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(dir);
    if (d == NULL) return 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (is_dot(e->d_name)) continue;
        ++n;
        if (fn != NULL) fn(dir, e->d_name, ctx);
    }
    closedir(d);
#endif
    return n;
}

static void make_dir(const char *path) {          /* an existing folder is fine */
#if defined(_WIN32)
    (void)_mkdir(path);
#else
    (void)mkdir(path, 0755);
#endif
}

static void remove_dir(const char *path) {
#if defined(_WIN32)
    (void)_rmdir(path);
#else
    (void)remove(path);
#endif
}

/* remove_all: a file goes with remove(), a folder is emptied first */
static void remove_tree(const char *path);
static void remove_entry(const char *dir, const char *name, void *ctx) {
    (void)ctx;
    char child[PATH_LEN];
    snprintf(child, sizeof child, "%s/%s", dir, name);
    if (remove(child) != 0) remove_tree(child);
}
static void remove_tree(const char *path) {
    for_each_entry(path, remove_entry, NULL);
    remove_dir(path);
}

static void print_name(const char *dir, const char *name, void *ctx) {
    (void)dir;
    (void)ctx;
    printf(" %s", name);                           /* BDF */
}
static void print_group(const char *root, const char *group, void *ctx) {
    (void)ctx;
    char devices[PATH_LEN];
    snprintf(devices, sizeof devices, "%s/%s/devices", root, group);
    printf("group %s:", group);
    for_each_entry(devices, print_name, NULL);     /* VFIO takes the whole group */
    printf("\n");
}
static void print_groups(const char *root) {      /* /sys/kernel/iommu_groups */
    for_each_entry(root, print_group, NULL);       /* no folder: prints nothing */
}

/* A BDF as a file name. Windows file names cannot hold a colon, so the practice tree uses an underscore there. */
static void bdf_name(const char *bdf, char *out, size_t cap) {
    snprintf(out, cap, "%s", bdf);
#if defined(_WIN32)
    for (char *c = out; *c != '\0'; ++c)
        if (*c == ':') *c = '_';
#endif
}

static const char *temp_dir(void) {
#if defined(_WIN32)
    const char *t = getenv("TEMP");
    return (t != NULL && t[0] != '\0') ? t : ".";
#else
    return "/tmp";
#endif
}

int main(void) {
    /* 1. a practice tree with the shape of sysfs, so the reader has something to read on any machine */
    char fake[PATH_LEN];
    snprintf(fake, sizeof fake, "%s/tomjnet_iommu_groups_model", temp_dir());
    remove_tree(fake);
    struct Dev { const char *group; const char *bdf; };
    static const struct Dev devs[] = {
        {"13", "0000:01:00.0"},                            /* an NVMe drive alone in its group */
        {"14", "0000:03:00.0"}, {"14", "0000:03:00.1"},    /* a GPU and its audio function: one group */
        {"15", "0000:06:00.0"}, {"15", "0000:07:00.0"},    /* two devices behind a switch without ACS */
    };
    bool built = true;
    make_dir(fake);
    for (size_t i = 0; i < sizeof devs / sizeof devs[0]; ++i) {
        char dir[PATH_LEN + 64], name[64], file_path[PATH_LEN + 192];
        snprintf(dir, sizeof dir, "%s/%s", fake, devs[i].group);
        make_dir(dir);
        snprintf(dir, sizeof dir, "%s/%s/devices", fake, devs[i].group);
        make_dir(dir);
        bdf_name(devs[i].bdf, name, sizeof name);
        snprintf(file_path, sizeof file_path, "%s/%s", dir, name);
        FILE *f = fopen(file_path, "w");               /* sysfs holds a link here; an empty file is enough */
        built = built && f != NULL;
        if (f != NULL) fclose(f);
    }
    printf("practice tree (typical groups, not this machine), in directory order:\n");
    if (built) {
        print_groups(fake);
    } else {
        printf("could not write under the temporary folder, skipped\n");
    }
#if defined(_WIN32)
    printf("(Windows file names cannot hold a colon: 0000_03_00.0 stands for 0000:03:00.0)\n");
#endif
    printf("group 14 and group 15 hold two devices each: VFIO assigns both or none\n");
    const bool counted = !built || for_each_entry(fake, NULL, NULL) == 3;
    remove_tree(fake);

    /* 2. the real thing */
    const char *real = "/sys/kernel/iommu_groups";
    printf("\nthis machine, %s:\n", real);
#if defined(__linux__)
    if (for_each_entry(real, NULL, NULL) == 0) {
        printf("no IOMMU groups visible here (IOMMU off in the firmware, a VM, WSL, or not Linux)\n");
        printf("on real Linux hardware with the IOMMU on: one line per group, as in the practice tree\n");
    } else {
        print_groups(real);
    }
#else
    printf("no IOMMU groups visible here (IOMMU off in the firmware, a VM, WSL, or not Linux)\n");
    printf("on real Linux hardware with the IOMMU on: one line per group, as in the practice tree\n");
#endif
    printf(counted ? "model check: OK\n" : "model check: FAILED\n");
    return counted ? 0 : 1;
}
