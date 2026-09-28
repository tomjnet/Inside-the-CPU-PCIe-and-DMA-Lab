#!/usr/bin/env bash
# NUMA Aware PCIe in 9 Minutes: NICs, NVMe and GPUs - slide 4: where the kernel tells you
# Look at: the node number of each device (-1 means the firmware did not say) and the CPU ranges next
# to it. Replace eth0 and the two BDFs with your own (ip link, lspci). Needs real Linux hardware;
# inside WSL these paths do not exist.
set -eu

# which NUMA node is the NIC wired to? (-1: the firmware did not say)
cat /sys/class/net/eth0/device/numa_node
#   1                                   (typical, two socket server)
# the CPUs of that node, in the format taskset accepts
cat /sys/class/net/eth0/device/local_cpulist
#   16-31,48-63                         (typical)
# the same two files for an NVMe drive and a GPU, by BDF
cat /sys/bus/pci/devices/0000:5e:00.0/numa_node
cat /sys/bus/pci/devices/0000:af:00.0/local_cpulist
