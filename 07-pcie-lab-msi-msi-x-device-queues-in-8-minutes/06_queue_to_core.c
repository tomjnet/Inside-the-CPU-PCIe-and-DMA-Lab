/* MSI, MSI-X and Device Queues in 8 Minutes - slide 6: queue to core: three mappings (C version of 06_queue_to_core.cpp) */
/* Build: make 06_queue_to_core_c */
#if defined(__linux__)
#define _GNU_SOURCE
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdbool.h>
#include <stdio.h>

#define QUEUES 4

/* one MSI-X vector per queue; its affinity names the core that runs
   the handler: /proc/irq/N/smp_affinity_list */
typedef struct { const char *name; int irq_core[QUEUES]; } Mapping;

/* worker q is pinned on core q and consumes queue q */
static const int worker_core[QUEUES] = { 0, 1, 2, 3 };

static const Mapping good = { "one queue per core, IRQ next to its worker",
                              { 0, 1, 2, 3 } };
static const Mapping piled = { "every vector on core 0", { 0, 0, 0, 0 } };
static const Mapping crossed = { "balanced, but IRQ core is never the worker core",
                                 { 1, 2, 3, 0 } };

static void show(const char *label, const Mapping *m) {
    printf("%s: %s\n", label, m->name);
    int local = 0;
    for (int q = 0; q < QUEUES; ++q) {
        const bool same = m->irq_core[q] == worker_core[q];
        if (same) ++local;
        printf("  queue %d -> MSI-X vector %d -> IRQ on core %d, worker on core %d%s", q, q, m->irq_core[q],
               worker_core[q], same ? "  local\n" : "  remote: cache lines move\n");
    }
    printf("  queues whose interrupt lands next to the worker: %d of %d\n\n", local, QUEUES);
}

int main(void) {
    printf("The model: 4 queues, 4 MSI-X vectors, 4 cores. On a real machine the irq_core column is\n"
           "what you read from /proc/irq/N/smp_affinity_list for the IRQ of each queue.\n\n");
    show("good", &good);
    show("piled", &piled);
    show("crossed", &crossed);
    printf("crossed is the trap: every core takes one vector, so the load looks healthy.\n");
    return 0;
}
