#!/usr/bin/env bash
# PCIe in 5 Minutes: How Devices Reach the CPU - slide 3: lspci -tv: read the tree
# Look at: which devices hang directly off a root port and which sit behind a switch (a bus range
# such as [02-05]). Needs real Linux hardware (sudo apt install pciutils); inside WSL the tree is empty.
set -eu

# the PCIe tree of this machine: -t tree, -v device names
lspci -tv
#   typical output, trimmed (yours will differ):
#   -[0000:00]-+-00.0  Host bridge: root complex
#              +-01.0-[01]----00.0  NVIDIA GPU
#              +-01.1-[02-05]----00.0-[03-05]--+-00.0-[04]----00.0  NIC
#              |                               \-01.0-[05]----00.0  NVMe
#              \-02.0-[06]----00.0  NVMe SSD
# how many devices the kernel sees (0 inside WSL)
lspci | wc -l
