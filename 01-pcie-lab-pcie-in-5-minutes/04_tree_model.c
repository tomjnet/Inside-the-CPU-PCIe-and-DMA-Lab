/* PCIe in 5 Minutes: How Devices Reach the CPU - slide 4: a c++ model of the tree (C version of 04_tree_model.cpp) */
/* Build: make 04_tree_model_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stddef.h>
#include <stdio.h>

/* C has no std::vector: the fabric is a fixed array plus an explicit count */
#define MAX_NODES 16
#define MAX_CHILDREN 8

typedef enum { KIND_ROOT_COMPLEX, KIND_SWITCH, KIND_ENDPOINT } Kind;
typedef struct {                 /* one device of the fabric */
    const char *name; Kind kind; /* "NIC", KIND_ENDPOINT */
    int parent;                  /* upstream node, -1 at the root */
    int children[MAX_CHILDREN];  /* one downstream link per child */
    int child_count;
} Node;
static Node tree[MAX_NODES];     /* node 0 is the root complex */
static int tree_size = 0;

static int add(const char *name, Kind kind, int parent) {   /* O(1) */
    if (tree_size >= MAX_NODES) return -1;
    if (parent >= 0) {
        if (tree[parent].child_count >= MAX_CHILDREN) return -1;
        tree[parent].children[tree[parent].child_count++] = tree_size;
    }
    tree[tree_size].name = name;
    tree[tree_size].kind = kind;
    tree[tree_size].parent = parent;
    tree[tree_size].child_count = 0;
    return tree_size++;          /* a link has exactly two ends */
}

static const char *kind_name(Kind k) {
    switch (k) {
        case KIND_ROOT_COMPLEX: return "root complex";
        case KIND_SWITCH:       return "switch";
        case KIND_ENDPOINT:     return "endpoint";
    }
    return "?";
}

/* the same picture lspci -tv draws: one line per device, indented by depth */
static void print_tree(int id, int depth) {
    printf("%*s%s%s  [%s]\n", depth * 4, "", depth > 0 ? "+- " : "", tree[id].name, kind_name(tree[id].kind));
    for (int i = 0; i < tree[id].child_count; ++i) print_tree(tree[id].children[i], depth + 1);
}

int main(void) {
    /* the tree of slide 3; root ports belong to the root complex, so they are folded into node 0 */
    int root = add("root complex (host bridge 00:00.0)", KIND_ROOT_COMPLEX, -1);
    int gpu  = add("GPU (behind root port 00:01.0)", KIND_ENDPOINT, root);
    int sw   = add("switch (behind root port 00:01.1)", KIND_SWITCH, root);
    int nic  = add("NIC", KIND_ENDPOINT, sw);
    int nvme = add("NVMe", KIND_ENDPOINT, sw);
    int ssd  = add("NVMe SSD (behind root port 00:02.0)", KIND_ENDPOINT, root);
    if (root < 0 || gpu < 0 || sw < 0 || nic < 0 || nvme < 0 || ssd < 0) {
        printf("FAIL: the fixed node table is full\n");
        return 1;
    }

    print_tree(root, 0);

    /* every node but the root has exactly one upstream link, so links = nodes - 1: a tree, never a shared bus */
    size_t endpoints = 0;
    size_t links = 0;
    for (int i = 0; i < tree_size; ++i) {
        if (tree[i].kind == KIND_ENDPOINT) ++endpoints;
        links += (size_t)tree[i].child_count;
    }
    printf("\nnodes %d, links %zu, endpoints %zu\n", tree_size, links, endpoints);
    printf("ids: gpu %d, switch %d, nic %d, nvme %d, ssd %d\n", gpu, sw, nic, nvme, ssd);
    printf("parent of the NIC: %s\n", tree[tree[nic].parent].name);

    if (links + 1 != (size_t)tree_size) {
        printf("FAIL: a tree has one link less than it has nodes\n");
        return 1;
    }
    return 0;
}
