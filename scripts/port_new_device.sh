#!/usr/bin/env bash
# Checklist script for porting CyberFlasher to a new device.
# Fill in the prompted values; it writes config/device.conf for you.

set -e
CONF="config/device.conf"
cp config/device.conf.example "$CONF"

read -rp "Device codename: " codename
read -rp "Board name: " board
read -rp "Super/boot partition real size in bytes (fastboot getvar partition-size:super, or lpdump): " supersize
read -rp "Uses dynamic (logical) partitions? [true/false]: " dynamic
read -rp "Ramdisk compression supported by kernel (check /proc/config.gz CONFIG_RD_*): " compression

sed -i "s/^DEVICE_CODENAME=.*/DEVICE_CODENAME=$codename/" "$CONF"
sed -i "s/^BOARD_NAME=.*/BOARD_NAME=$board/" "$CONF"
sed -i "s/^USES_DYNAMIC_PARTITIONS=.*/USES_DYNAMIC_PARTITIONS=$dynamic/" "$CONF"
sed -i "s/^RAMDISK_COMPRESSION=.*/RAMDISK_COMPRESSION=$compression/" "$CONF"

echo "Wrote $CONF. Still needed by hand:"
echo "  - PATH_* by-name entries for this device's partitions"
echo "  - A flash_engine.c unpack/repack backend if this SoC isn't already supported"
echo "  - PARTITION_SIZE_PRESETS if 32/64/128MB presets don't match this device's partitions"
