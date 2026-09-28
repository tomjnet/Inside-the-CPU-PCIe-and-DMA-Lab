#!/usr/bin/env bash
# Build an End to End Low Latency I/O Map in 10 Minutes - slide 5: the audit, part one: link, numa node, iommu
# Look at: LnkSta must equal LnkCap (a smaller width or speed is a down trained link), numa_node must be the
# node of your threads (-1: not reported), and the IOMMU type tells translated (DMA) from passthrough (identity).
# Needs real Linux hardware and pciutils; inside WSL there are no PCIe devices. Set DEV to your own device.
set -eu

DEV=0000:3b:00.0          # the device under audit, typical output
# 1. link status: negotiated against capable
sudo lspci -s $DEV -vv | grep -E "LnkCap:|LnkSta:"
#   LnkCap: Port #0, Speed 16GT/s, Width x8
#   LnkSta: Speed 16GT/s, Width x8
# 2. NUMA node and the CPUs next to the device
cd /sys/bus/pci/devices/$DEV && cat numa_node local_cpulist
#   0
#   0-15,32-47
# 3. IOMMU mode: DMA is translated, identity is passthrough
cat /sys/bus/pci/devices/$DEV/iommu_group/type
#   identity
