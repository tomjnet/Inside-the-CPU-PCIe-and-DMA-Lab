#!/usr/bin/env bash
# IOMMU in 8 Minutes: Translation and Isolation for Devices - slide 7: is the iommu on?
# Look at: the "IOMMU enabled" line, the group each device joined, and the type file of a group (DMA is
# translated, identity is passthrough). Needs real Linux hardware; inside WSL or a plain VM there are no groups.
set -eu

# Is the IOMMU on? DMAR is Intel VT-d, AMD-Vi is AMD
sudo dmesg | grep -i -e DMAR -e IOMMU || echo "no DMAR or IOMMU lines: IOMMU off, a VM or WSL"
#   DMAR: IOMMU enabled                                  (typical)
#   pci 0000:03:00.0: Adding to iommu group 14           (typical)
# Kernel options: intel_iommu=on, amd_iommu=on, iommu=pt
cat /proc/cmdline
# One folder per group, one link per device in the group
ls /sys/kernel/iommu_groups
# group 14 is the example of the slide: pass the number of one of your own groups as the first argument
GROUP="${1:-14}"
ls "/sys/kernel/iommu_groups/$GROUP/devices"
#   0000:03:00.0                                         (typical)
# Domain type of the group: DMA is translated, identity is passthrough
cat "/sys/kernel/iommu_groups/$GROUP/type"
