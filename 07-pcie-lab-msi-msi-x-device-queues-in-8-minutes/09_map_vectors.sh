#!/usr/bin/env bash
# MSI, MSI-X and Device Queues in 8 Minutes - slide 9: map vectors to cpus
# Look at: the core list of each IRQ before and after the write. Replace eth0 and the IRQ numbers 45 to 48 with
# the ones 08_nic_queues.sh printed on your machine. The last two commands need root and real Linux hardware.
set -eu

# the RSS indirection table: hash entry to queue (typical)
ethtool -x eth0
#   0:  0 1 2 3 0 1 2 3
#   8:  0 1 2 3 0 1 2 3

# which core takes each vector: read the affinity of IRQ 45 to 48
for irq in 45 46 47 48; do cat /proc/irq/$irq/smp_affinity_list; done

# move queue 1 (IRQ 46) to core 1; stop irqbalance or it moves it back
sudo systemctl stop irqbalance
echo 1 | sudo tee /proc/irq/46/smp_affinity_list
