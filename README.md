# Inside the CPU: PCIe and DMA Lab

Runnable code from the [Inside the CPU: PCIe and DMA Lab](https://www.youtube.com/@TomJNet) video series: PCIe topology, lanes and link width, configuration space, BARs and MMIO, DMA and descriptor rings, the IOMMU, MSI-X queues and NUMA locality.
One folder per episode, one complete C++20 program per idea shown on the slides, and next to each one its C17 twin (`NN_name.c` beside `NN_name.cpp`) that teaches the same idea in plain C, plus the shell scripts that run the Linux tools of the episode.

| Name | Description | Folder |
|---|---|---|
| PCIe in 5 Minutes: How Devices Reach the CPU | Episode 1 of the PCIe and DMA Lab. Every fast device in your machine, the network card, the NVMe drive, the GPU, reaches the CPU and the memory through PCI Express. In five minutes: why PCIe is a point to point packet fabric and not a shared bus, the three roles of the tree (root complex, switch, endpoint), how to read the tree of a real machine with lspci -tv, and a small C++ model of that tree that prints the path from any endpoint to the root complex, so you can see which devices share a link. The topology, the paths and the map of the playlist are animated. | `01-pcie-lab-pcie-in-5-minutes` |
| PCIe Lanes, Generations and Link Width in 7 Minutes | How fast is one PCIe link, and is yours running as fast as it can? Episode 2 of the PCIe and DMA Lab zooms into a single branch of the PCIe tree. A lane is one differential pair each way, a link is x1, x4, x8 or x16 lanes side by side, and every generation doubles the rate per lane: 2.5, 5, 8, 16 and 32 GT/s. We separate the signaling rate from the payload bandwidth (8b/10b encoding in Gen 1 and 2, 128b/130b from Gen 3), build a small C++ calculator that prints the table, read LnkCap and LnkSta with lspci and sysfs, and write a parser that flags a down trained link and says how much bandwidth it lost. The lanes and the down trained link are animated. | `02-pcie-lab-lanes-generations-link-width-in-7-minutes` |
| PCIe Enumeration and Configuration Space in 7 Minutes | How does a computer find out which PCIe devices it has, and how do you name one of them precisely? Episode 3 of the PCIe and DMA Lab covers the bus, device, function address (BDF), the depth first scan that firmware and the kernel run at boot to hand out bus numbers, the configuration space that every function exposes (256 bytes in PCI, 4096 bytes in PCIe, with a standard header in the first 64 bytes) and the capability linked list that holds everything optional, from MSI-X to the PCI Express link registers. In the lab we trace one device from its BDF to its kernel driver and its sysfs directory. The BDF split, the scan, the header and the capability list are animated. | `03-pcie-lab-enumeration-configuration-space-in-7-minutes` |
| BARs and MMIO in 8 Minutes: Talking to a Device | Episode 4 of the PCIe and DMA Lab. How a driver actually talks to a PCIe device, in eight minutes. A Base Address Register (BAR) is a request for address space; the firmware assigns the range, and from then on a plain load or store inside that window becomes a PCIe packet that lands on a device register. That is memory mapped I/O (MMIO). We size a BAR with the all ones trick, compare MMIO with the legacy port I/O of x86, see why device registers need volatile, strict ordering and no caching, and why doorbells keep register writes to one per batch. Then we build a register block model in C++ and read the resource file of a real device. The address map and the doorbell are animated. Numbers are typical orders of magnitude: run the samples and read your own. | `04-pcie-lab-bars-and-mmio-in-8-minutes` |
| DMA in 8 Minutes: Devices Move Data Themselves | Episode 5 of the PCIe and DMA Lab. Registers are fine for control and far too slow for data, so fast devices move the data themselves. In eight minutes: why programmed I/O keeps a core busy and how a bus master endpoint reads and writes host RAM on its own, what a pinned buffer, a DMA address and a descriptor are, how the descriptor ring works with head, tail and a doorbell, how ownership of a buffer passes between driver and device without locks, scatter and gather lists, and coherent against streaming mappings. The demo is a NIC receive queue as a small C++ model: host RAM as an array, the driver that posts buffers and harvests packets, and the device as a step function that fills them. The programmed I/O against DMA picture, the ring, the ownership cycle and the gather are animated. | `05-pcie-lab-dma-in-8-minutes` |
| IOMMU in 8 Minutes: Translation and Isolation for Devices | Episode 6 of the PCIe and DMA Lab. In the last episode the device became a bus master and moved data by itself; this one asks what stops it from writing anywhere in RAM. The answer is the IOMMU, a memory management unit for devices. We look at DMA without an IOMMU (silent corruption, hostile devices, guests that do not know physical addresses), the three kinds of address (virtual, physical and I/O virtual, the IOVA), how the requester ID selects a domain with page tables of its own, how a bad DMA becomes a logged fault, what translation costs (IOTLB misses, map and unmap, strict and lazy invalidation), and how to inspect the IOMMU on Linux: the boot log, the kernel command line, IOMMU groups, VFIO and the iommu=pt passthrough mode. The paths through the IOMMU are animated, and two small C++ models let you repeat the ideas without touching real hardware. | `06-pcie-lab-iommu-in-8-minutes` |
| MSI, MSI-X and Device Queues in 8 Minutes | How a PCIe device tells the CPU that work is done, in eight minutes. Legacy INTx shares a line between devices; MSI and MSI-X turn the interrupt into a small memory write, and MSI-X keeps one table entry per vector inside a BAR, so every vector can target its own core. Multiqueue devices use that to give each queue its own vector: RX and TX queues on a NIC, queue pairs on an NVMe drive. RSS hashes each flow to a queue, and the affinity of the vector decides which core pays for the interrupt. A small C++ model hashes flows to queues and queues to cores and prints the per core load of a good, a piled and a crossed mapping, and the lab scripts read the same facts from a real NIC with ethtool and /proc/interrupts. Episode 7 of the PCIe and DMA Lab. | `07-pcie-lab-msi-msi-x-device-queues-in-8-minutes` |
| NUMA Aware PCIe in 9 Minutes: NICs, NVMe and GPUs | Episode 8 of the PCIe and DMA Lab. On a server with two sockets every PCIe slot is wired to the root complex of one socket, and so to one NUMA node. When the DMA buffer, the interrupt or the worker thread sits on the other node, every packet crosses the inter socket link, and the p99 shows it. This episode shows where the kernel publishes the node of a device (numa_node and local_cpulist in sysfs), how to align the device, its memory, its IRQs and your threads on one node, and how peer to peer DMA, the idea behind GPUDirect, lets one device write into the BAR of another without touching host memory. The two socket paths, the four pieces and the peer to peer path are animated. | `08-pcie-lab-numa-aware-pcie-in-9-minutes` |
| Build an End to End Low Latency I/O Map in 10 Minutes | The last episode of the PCIe and DMA Lab puts the whole playlist on one page. Take one device, a network card or an NVMe drive, and write down six facts: link status, NUMA node, IOMMU mode, queue count, IRQ affinity and ring sizes. That page is the I/O map. Then turn the three knobs the map exposes, queue depth, batching and backpressure, and measure the median and the tail before and after every change. The audit is a short shell script, the map is a small C++ program that reads sysfs (and falls back to built in sample data when no PCIe device is visible, as in WSL), and the knobs are a model you can run. The progression, the map, the knobs and the checklist are animated. | `09-pcie-lab-end-to-end-io-map-in-10-minutes` |

## The lab

- C++20 with `g++` on Ubuntu 24.04, x86 64 hardware. WSL Ubuntu is fine for writing and running most samples.
- Take timings on real Linux hardware: a virtual machine shares cores and hides the counters `perf` reads, and WSL shows no real PCIe devices, IRQs or NUMA nodes.
- Linux system calls sit inside `#if defined(__linux__)`. On Windows the programs still build and run, and print what the Linux branch would show.
- Every timing program prints its own numbers for the machine it runs on and never fails because of them. A call the system refuses (no root, one NUMA node, a filesystem without direct I/O) prints why and moves on.
- Hardware a program cannot touch is a small C++ model with deterministic output: read it, run it, change it.

## Build and run

Ubuntu or WSL Ubuntu:

```bash
sudo apt update
sudo apt install build-essential make linux-tools-common linux-tools-generic
```

Every folder has the same Makefile, for both languages. It picks the compilers by operating system: MSVC on Windows (run from a Developer Command Prompt), g++ and gcc on Linux, clang++ and clang on macOS.

```bash
cd 05-pcie-lab-dma-in-8-minutes
make STD=c++20          # build everything
make run STD=c++20      # build and run every sample
make cpp STD=c++20      # only the C++ samples
make c                  # only the C samples (C17; make CSTD=c11 to change)
make run-NN_name_c      # one C sample: NN_name.c builds NN_name_c
make clean
```

The C samples build with `gcc -std=c17 -Wall -Wextra -pedantic -O2 -pthread` on Linux and MinGW, and with `cl /std:c17 /experimental:c11atomics /W4` on Windows. They use C11 `<threads.h>` and `<stdatomic.h>`; MinGW has no `<threads.h>`, so the files that start threads carry a ten line mapping onto pthreads.

The `.sh` scripts need Linux; some commands in them need `sudo` or real hardware and say so.

## License

MIT
