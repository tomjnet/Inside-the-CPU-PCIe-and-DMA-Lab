#!/usr/bin/env bash
# Build an End to End Low Latency I/O Map in 10 Minutes - slide 6: the audit, part two: queues, irqs, rings
# Look at: the current queue count against the local cores, one local CPU per queue vector, and a ring size
# that holds one burst. Needs a real NIC and ethtool; set NIC to your own interface (ip link shows them).
set -eu

NIC=eth0                  # the interface of that device, typical
# 4. queue count: maximum first, then the current setting
ethtool -l $NIC | grep Combined
#   Combined: 16        Combined: 8
# 5. IRQ affinity: the CPU that takes each queue vector
for i in $(grep $NIC /proc/interrupts | cut -d: -f1); do
  echo "irq $i: cpu $(cat /proc/irq/$i/smp_affinity_list)"
done
#   irq 98: cpu 0       irq 99: cpu 1       irq 100: cpu 2
# 6. ring sizes: maximum first, then the current setting
ethtool -g $NIC | grep -E "^RX:|^TX:"
#   RX: 4096  TX: 4096  RX: 1024  TX: 1024
