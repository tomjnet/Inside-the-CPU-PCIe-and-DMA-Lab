// PCIe in 5 Minutes: How Devices Reach the CPU - slide 4: a c++ model of the tree
// Build: make 04_tree_model
#include <cstddef>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

enum class Kind { RootComplex, Switch, Endpoint };
struct Node {                    // one device of the fabric
    std::string name; Kind kind; // "NIC", Kind::Endpoint
    int parent;                  // upstream node, -1 at the root
    std::vector<int> children;   // one downstream link per child
};
std::vector<Node> tree;          // node 0 is the root complex
int add(std::string name, Kind kind, int parent) {      // O(1)
    if (parent >= 0) tree[parent].children.push_back(int(tree.size()));
    tree.push_back({std::move(name), kind, parent, {}});
    return int(tree.size()) - 1; // a link has exactly two ends
}

static const char* kind_name(Kind k) {
    switch (k) {
        case Kind::RootComplex: return "root complex";
        case Kind::Switch:      return "switch";
        case Kind::Endpoint:    return "endpoint";
    }
    return "?";
}

// the same picture lspci -tv draws: one line per device, indented by depth
static void print_tree(int id, int depth) {
    std::cout << std::string(static_cast<std::size_t>(depth) * 4, ' ') << (depth > 0 ? "+- " : "")
              << tree[id].name << "  [" << kind_name(tree[id].kind) << "]\n";
    for (int child : tree[id].children) print_tree(child, depth + 1);
}

int main() {
    // the tree of slide 3; root ports belong to the root complex, so they are folded into node 0
    int root = add("root complex (host bridge 00:00.0)", Kind::RootComplex, -1);
    int gpu  = add("GPU (behind root port 00:01.0)", Kind::Endpoint, root);
    int sw   = add("switch (behind root port 00:01.1)", Kind::Switch, root);
    int nic  = add("NIC", Kind::Endpoint, sw);
    int nvme = add("NVMe", Kind::Endpoint, sw);
    int ssd  = add("NVMe SSD (behind root port 00:02.0)", Kind::Endpoint, root);

    print_tree(root, 0);

    // every node but the root has exactly one upstream link, so links = nodes - 1: a tree, never a shared bus
    std::size_t endpoints = 0;
    std::size_t links = 0;
    for (const Node& n : tree) {
        if (n.kind == Kind::Endpoint) ++endpoints;
        links += n.children.size();
    }
    std::cout << "\nnodes " << tree.size() << ", links " << links << ", endpoints " << endpoints << "\n";
    std::cout << "ids: gpu " << gpu << ", switch " << sw << ", nic " << nic << ", nvme " << nvme << ", ssd " << ssd
              << "\n";
    std::cout << "parent of the NIC: " << tree[tree[nic].parent].name << "\n";

    if (links + 1 != tree.size()) {
        std::cout << "FAIL: a tree has one link less than it has nodes\n";
        return 1;
    }
    return 0;
}
