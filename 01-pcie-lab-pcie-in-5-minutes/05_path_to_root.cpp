// PCIe in 5 Minutes: How Devices Reach the CPU - slide 5: the path from an endpoint to the root
// Build: make 05_path_to_root
#include <cstddef>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

// the model of slide 4
enum class Kind { RootComplex, Switch, Endpoint };
struct Node {
    std::string name; Kind kind;
    int parent;
    std::vector<int> children;
};
std::vector<Node> tree;
int add(std::string name, Kind kind, int parent) {
    if (parent >= 0) tree[parent].children.push_back(int(tree.size()));
    tree.push_back({std::move(name), kind, parent, {}});
    return int(tree.size()) - 1;
}

// every packet of an endpoint climbs this path: one link per hop
std::vector<int> path_to_root(int endpoint) {
    std::vector<int> path;
    for (int n = endpoint; n != -1; n = tree[n].parent)
        path.push_back(n);       // O(depth): 1 to 3 links is typical
    return path;                 // endpoint first, root complex last
}
// links crossed = path.size() - 1; a switch adds one more crossing

// endpoints at or below a node: the upstream link of that node carries the traffic of all of them
static int endpoints_below(int id) {
    int count = tree[id].kind == Kind::Endpoint ? 1 : 0;
    for (int child : tree[id].children) count += endpoints_below(child);
    return count;
}

int main() {
    int root = add("root complex", Kind::RootComplex, -1);
    add("GPU", Kind::Endpoint, root);
    int sw = add("switch", Kind::Switch, root);
    add("NIC", Kind::Endpoint, sw);
    add("NVMe", Kind::Endpoint, sw);
    add("NVMe SSD", Kind::Endpoint, root);

    int id = 0;
    bool ok = true;
    for (const Node& n : tree) {
        if (n.kind == Kind::Endpoint) {
            std::vector<int> path = path_to_root(id);
            std::size_t links = path.size() - 1;
            int shared = 0;
            std::cout << n.name << ": ";
            for (std::size_t i = 0; i < path.size(); ++i) {
                std::cout << (i > 0 ? " -> " : "") << tree[path[i]].name;
                // the link above path[i] is shared when more than one endpoint sits at or below path[i]
                if (i + 1 < path.size() && endpoints_below(path[i]) > 1) ++shared;
            }
            std::cout << "\n    links crossed " << links << ", shared with another endpoint " << shared << "\n";
            if (path.empty() || path.back() != root) ok = false;   // every path must end at the root complex
        }
        ++id;
    }
    std::cout << "\nthe NIC and the NVMe drive cross the upstream link of the switch together;\n"
                 "the GPU and the NVMe SSD own their single link to a root port\n";
    if (!ok) {
        std::cout << "FAIL: a path did not end at the root complex\n";
        return 1;
    }
    return 0;
}
