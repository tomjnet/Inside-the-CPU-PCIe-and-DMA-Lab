#!/usr/bin/env bash
# DMA in 8 Minutes: Devices Move Data Themselves - slide 9: find DMA on a real machine
# Look for BusMaster+ on the Control line, the current against the maximum RX ring size, and drop counters
# that rise while traffic flows. Put your own device (lspci) and interface (ip link) in place of 03:00.0 and
# eth0. Needs real Linux hardware: WSL and most virtual machines show no PCIe devices. "|| true" keeps the
# script going when a grep finds nothing.
set -eu

# May the device master the bus? BusMaster+ means DMA is on
lspci -vv -s 03:00.0 | grep -i busmaster || true
#   Control: I/O- Mem+ BusMaster+ SpecCycle- MemWINV- (typical)
# Ring sizes: descriptors the driver posts, and the maximum
ethtool -g eth0 || true
#   RX: 4096 (maximum)    RX: 512 (current)    (typical)
# Packets dropped because the ring ran out of buffers
ethtool -S eth0 | grep -i -e drop -e miss -e no_buf || true
# DMA limits and bounce buffers the kernel reports at boot
sudo dmesg | grep -i -e "dma" -e swiotlb || true
