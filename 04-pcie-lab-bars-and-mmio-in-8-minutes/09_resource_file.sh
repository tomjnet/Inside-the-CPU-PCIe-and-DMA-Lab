#!/usr/bin/env bash
# BARs and MMIO in 8 Minutes: Talking to a Device - slide 9: the resource file of a real device
# Look at: one Region line per BAR (address, 32 or 64 bit, prefetchable, size); end - start + 1 in the sysfs
# resource file gives the same size. Replace 03:00.0 with a BDF of your lspci. Needs real Linux hardware: WSL shows none.
set -eu

# the BARs of one device as lspci decodes them (typical output)
lspci -vv -s 03:00.0 | grep -i 'region'
#   Region 0: Memory at fc500000 (64-bit, non-prefetchable) [size=16K]

# the same from sysfs: start, end and flags, one line per BAR
cat /sys/bus/pci/devices/0000:03:00.0/resource
#   0x00000000fc500000 0x00000000fc503fff 0x0000000000140204
#   0x0000000000000000 0x0000000000000000 0x0000000000000000

# where the kernel placed every window in the CPU address map
sudo grep -i -e 'pci' -e 'nvme' /proc/iomem | head
#   fc500000-fc503fff : nvme
