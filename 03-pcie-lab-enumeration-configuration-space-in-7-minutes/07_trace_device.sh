#!/usr/bin/env bash
# Look at: the numbers in brackets of lspci -nn (class code, then vendor:device), the "Kernel driver in use"
# line, and the driver link in sysfs that names the same driver. WSL shows few or no PCIe devices.
set -eu

# The slide traces 03:00.0. Pass the address of one of your own devices: ./07_trace_device.sh 01:00.0
DEV="${1:-03:00.0}"

# every function: BDF, [class code], [vendor:device] (typical)
lspci -nn || echo "  (lspci is missing: sudo apt install pciutils)"
#   03:00.0 Ethernet controller [0200]: Intel I210 [8086:1533]
# which kernel driver claimed it
lspci -k -s "$DEV" || true
#   Kernel driver in use: igb
# the same function in sysfs: identity files and the driver link
ls "/sys/bus/pci/devices/0000:$DEV/" || echo "  (no function 0000:$DEV here: pick one from lspci -nn)"
readlink "/sys/bus/pci/devices/0000:$DEV/driver" || echo "  (no driver bound, or no such function)"
#   ../../../../bus/pci/drivers/igb
# its capability list, decoded; root sees all of it
sudo lspci -vv -s "$DEV" | grep Capabilities || echo "  (no capabilities listed for $DEV)"
