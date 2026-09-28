/* PCIe in 5 Minutes: How Devices Reach the CPU - slide 5: the path from an endpoint to the root (C version of 05_path_to_root.cpp) */
/* Build: make 05_path_to_root_c */
#if defined(__linux__)
#define _GNU_SOURCE                  /* sched_setaffinity, CPU_SET, O_DIRECT, clock_gettime */
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS      /* fopen, strerror: MSVC deprecates the standard names */
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* the model of slide 4; C has no std::vector, so fixed arrays plus explicit counts */
#define MAX_NODES 16
#define MAX_CHILDREN 8

typedef enum { KIND_ROOT_COMPLEX, KIND_SWITCH, KIND_ENDPOINT } Kind;
typedef struct {
    const char *name; Kind kind;
    int parent;
    int children[MAX_CHILDREN];
    int child_count;
} Node;
static Node tree[MAX_NODES];
static int tree_size = 0;

static int add(const char *name, Kind kind, int parent) {
    if (tree_size >= MAX_NODES) return -1;
    if (parent >= 0) {
        if (tree[parent].child_count >= MAX_CHILDREN) return -1;
        tree[parent].children[tree[parent].child_count++] = tree_size;
    }
    tree[tree_size].name = name;
    tree[tree_size].kind = kind;
    tree[tree_size].parent = parent;
    tree[tree_size].child_count = 0;
    return tree_size++;
}

/* every packet of an endpoint climbs this path: one link per hop; the caller owns the buffer */
static size_t path_to_root(int endpoint, int *path, size_t cap) {
    size_t len = 0;
    for (int n = endpoint; n != -1 && len < cap; n = tree[n].parent)
        path[len++] = n;         /* O(depth): 1 to 3 links is typical */
    return len;                  /* endpoint first, root complex last */
}
/* links crossed = path length - 1; a switch adds one more crossing */

/* endpoints at or below a node: the upstream link of that node carries the traffic of all of them */
static int endpoints_below(int id) {
    int count = tree[id].kind == KIND_ENDPOINT ? 1 : 0;
    for (int i = 0; i < tree[id].child_count; ++i) count += endpoints_below(tree[id].children[i]);
    return count;
}

int main(void) {
    int root = add("root complex", KIND_ROOT_COMPLEX, -1);
    add("GPU", KIND_ENDPOINT, root);
    int sw = add("switch", KIND_SWITCH, root);
    add("NIC", KIND_ENDPOINT, sw);
    add("NVMe", KIND_ENDPOINT, sw);
    add("NVMe SSD", KIND_ENDPOINT, root);

    bool ok = true;
    for (int id = 0; id < tree_size; ++id) {
        const Node *n = &tree[id];
        if (n->kind == KIND_ENDPOINT) {
            int path[MAX_NODES];
            size_t len = path_to_root(id, path, MAX_NODES);
            size_t links = len > 0 ? len - 1 : 0;
            int shared = 0;
            printf("%s: ", n->name);
            for (size_t i = 0; i < len; ++i) {
                printf("%s%s", i > 0 ? " -> " : "", tree[path[i]].name);
                /* the link above path[i] is shared when more than one endpoint sits at or below path[i] */
                if (i + 1 < len && endpoints_below(path[i]) > 1) ++shared;
            }
            printf("\n    links crossed %zu, shared with another endpoint %d\n", links, shared);
            if (len == 0 || path[len - 1] != root) ok = false;   /* every path must end at the root complex */
        }
    }
    printf("\nthe NIC and the NVMe drive cross the upstream link of the switch together;\n"
           "the GPU and the NVMe SSD own their single link to a root port\n");
    if (!ok) {
        printf("FAIL: a path did not end at the root complex\n");
        return 1;
    }
    return 0;
}
