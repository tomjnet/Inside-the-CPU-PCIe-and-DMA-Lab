#!/usr/bin/env bash
# Build an End to End Low Latency I/O Map in 10 Minutes - slide 12: the repeatable checklist
# Look at: the six facts of one device on one page, then measure. Usage: ./audit.sh <BDF> [<nic>], for example
# ./audit.sh 0000:3b:00.0 eth0 (lspci -D shows the BDFs, ip link the interfaces). Needs real Linux hardware;
# inside WSL there are no PCIe devices and the script says so. Keep it in your repository and run it again
# after every BIOS, kernel or hardware change.
set -eu

DEV=${1:-0000:3b:00.0}
NIC=${2:-}
SYS=/sys/bus/pci/devices/$DEV
if [ ! -d "$SYS" ]; then
  echo "no PCIe device $DEV here (WSL, a container, or a wrong BDF): nothing to audit"
  exit 0
fi

# 1. link status: negotiated against capable (sysfs needs no root; lspci -vv shows LnkCap and LnkSta)
echo "1 link    now x$(cat $SYS/current_link_width) at $(cat $SYS/current_link_speed)"
echo "          max x$(cat $SYS/max_link_width) at $(cat $SYS/max_link_speed)"
# 2. NUMA node and the CPUs next to the device
echo "2 NUMA    node $(cat $SYS/numa_node), local CPUs $(cat $SYS/local_cpulist)"
# 3. IOMMU mode: DMA is translated, identity is passthrough
if [ -r $SYS/iommu_group/type ]; then
  echo "3 IOMMU   $(cat $SYS/iommu_group/type)"
else
  echo "3 IOMMU   off or not visible"
fi
# 4 and 5. one line per MSI or MSI-X vector: the CPUs that may take it
if [ -d $SYS/msi_irqs ]; then
  echo "4 queues  $(ls $SYS/msi_irqs | wc -l) vectors"
  for i in $(ls $SYS/msi_irqs); do
    echo "5 IRQ     $i: cpu $(cat /proc/irq/$i/smp_affinity_list 2>/dev/null || echo unknown)"
  done
else
  echo "4 queues  no MSI vectors (no driver bound?)"
fi
# 6. ring sizes and channels of a NIC, when one was given
if [ -n "$NIC" ]; then
  ethtool -l $NIC | grep Combined
  ethtool -g $NIC | grep -E "^RX:|^TX:"
fi
echo "7 measure p50, p99, p99.9 under the real load"
echo "8 change one knob, measure again, write it down"
