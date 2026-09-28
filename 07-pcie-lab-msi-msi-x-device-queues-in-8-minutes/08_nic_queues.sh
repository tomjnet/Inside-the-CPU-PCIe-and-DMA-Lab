#!/usr/bin/env bash
# MSI, MSI-X and Device Queues in 8 Minutes - slide 8: inspect a multiqueue nic
# Look at: one /proc/interrupts row per queue and a diagonal of counters (queue 0 on CPU 0, queue 1 on CPU 1).
# Replace eth0 with your interface (ip link). Needs real Linux hardware: WSL shows no PCIe devices or MSI-X rows.
set -eu

# how many queues the NIC offers and how many are in use (typical)
ethtool -l eth0
#   Pre-set maximums:            Combined: 8
#   Current hardware settings:   Combined: 4

# one MSI-X vector per queue: IRQ, count per CPU, type, name (typical)
grep eth0 /proc/interrupts
#   45:  91022      0      0      0  IR-PCI-MSIX  eth0-TxRx-0
#   46:      0  88710      0      0  IR-PCI-MSIX  eth0-TxRx-1
#   47:      0      0  90133      0  IR-PCI-MSIX  eth0-TxRx-2
#   48:      0      0      0  89542  IR-PCI-MSIX  eth0-TxRx-3
