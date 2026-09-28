#!/usr/bin/env bash
# PCIe Lanes, Generations and Link Width in 7 Minutes - slide 5: LnkCap against LnkSta
# Look at: Speed and Width in LnkCap (what the device can do) against LnkSta (what the link got).
# Replace 01:00.0 with a device of your own from lspci. WSL shows no PCIe devices: run this on real Linux.
set -eu

# LnkCap: what the device can do. LnkSta: what the link got
sudo lspci -vv -s 01:00.0 | grep -E "LnkCap:|LnkSta:"
#   typical output, a Gen 4 x16 card that trained down:
#   LnkCap: Port #0, Speed 16GT/s, Width x16, ASPM L1
#   LnkSta: Speed 8GT/s (downgraded), Width x4 (downgraded)

# the same four numbers from sysfs, no root needed
dev=/sys/bus/pci/devices/0000:01:00.0
cat $dev/max_link_speed $dev/max_link_width
cat $dev/current_link_speed $dev/current_link_width

# not on the slide: every device at once, flagged when the link is below its maximum
# (an idle device may lower its speed to save power: check again under load)
for d in /sys/bus/pci/devices/*; do
    [ -r "$d/max_link_width" ] || continue
    cap="$(cat "$d/max_link_speed") x$(cat "$d/max_link_width")"
    sta="$(cat "$d/current_link_speed") x$(cat "$d/current_link_width")"
    flag=""
    [ "$cap" = "$sta" ] || flag="   below its maximum"
    echo "$(basename "$d")  cap $cap  sta $sta$flag"
done

# not on the slide: feed the two lines of a real device to the parser of slide 7
# ./07_parse_lnksta "$(sudo lspci -vv -s 01:00.0 | grep LnkCap:)" "$(sudo lspci -vv -s 01:00.0 | grep LnkSta:)"
