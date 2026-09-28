#!/usr/bin/env bash
# NUMA Aware PCIe in 9 Minutes: NICs, NVMe and GPUs - slide 10: see it and fix it: lstopo and numactl
# Look at: under which NUMANode the NIC, the NVMe drives and the GPU hang, and the distance table
# (10 is local). Needs sudo apt install hwloc numactl, a machine with two nodes, and root for the
# last line; ./worker, node 1, CPUs 16-31 and IRQ 128 are examples, use your own.
set -eu

# the machine as a tree: nodes, caches and the PCIe devices under them
lstopo --of console
#   NUMANode L#1 (P#1 128GB)                     (typical, trimmed)
#     HostBridge  PCI 5e:00.0 (Ethernet)  Net "eth0"
# nodes, their CPUs, their memory and the distance table
numactl --hardware
#   node distances:   0: 10 21    1: 21 10       (typical)
# run the worker on the node of the NIC, threads and memory together
numactl --cpunodebind=1 --membind=1 ./worker
# the IRQ of one queue on the local CPUs (as root)
echo 16-31 > /proc/irq/128/smp_affinity_list
