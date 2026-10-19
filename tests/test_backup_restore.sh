#!/usr/bin/env bash
# test_backup_restore.sh — Phase 3 proof: a real backup -> corrupt ->
# restore round trip against synthetic "partitions" (regular files
# standing in for block devices — dd behaves identically on both) and a
# synthetic mounted /mnt/system directory (for the file-level tar path).
# Confirms cf_restore_run is no longer a no-op and genuinely restores
# both flat images and filesystems, and that it correctly REFUSES a
# backup with a tampered .md5 sidecar rather than restoring it blindly.
set -e
cd "$(dirname "$0")/.."

REAL_HEADER_DIR=""
for dir in /usr/include /usr/local/include; do
    if [ -f "$dir/archive.h" ] && [ -f "$dir/archive_entry.h" ]; then
        REAL_HEADER_DIR="$dir"; break
    fi
done
LIBARCHIVE_PATH=""
for candidate in /usr/lib/x86_64-linux-gnu/libarchive.so /usr/lib/libarchive.so \
                  /usr/lib/x86_64-linux-gnu/libarchive.so.13 /usr/lib/libarchive.so.13; do
    if [ -f "$candidate" ]; then LIBARCHIVE_PATH="$candidate"; break; fi
done
if [ -n "$REAL_HEADER_DIR" ]; then
    ARCHIVE_INCLUDE=""; ARCHIVE_LINK="-larchive"
elif [ -n "$LIBARCHIVE_PATH" ]; then
    ARCHIVE_INCLUDE="-Itests/archive_shim"; ARCHIVE_LINK="$LIBARCHIVE_PATH"
else
    echo "SKIPPED: no libarchive available at all"
    exit 0
fi

FIXTURES=/tmp/cf_backup_test_fixtures
rm -rf "$FIXTURES" /mnt/system /tmp/cf_backup_test_dest
mkdir -p "$FIXTURES" /tmp/cf_backup_test_dest

echo "=== Creating synthetic partition files (regular files standing in for block devices) ==="
head -c 4096 /dev/urandom > "$FIXTURES/boot_partition.img"
head -c 2048 /dev/urandom > "$FIXTURES/recovery_partition.img"

cat > "$FIXTURES/fstab" << EOF
/boot              emmc     $FIXTURES/boot_partition.img
/recovery          emmc     $FIXTURES/recovery_partition.img
EOF

echo "=== Compiling test ==="
gcc -Wall -Wextra -D_POSIX_C_SOURCE=200809L $ARCHIVE_INCLUDE -Isrc/backup -Isrc/partition \
    -o /tmp/cf_test_backup_restore \
    tests/test_backup_restore.c \
    src/backup/backup_engine.c src/backup/twrp_tar.c src/backup/twrp_digest.c \
    src/partition/fstab_parser.c \
    $ARCHIVE_LINK

echo "=== Running round trip ==="
/tmp/cf_test_backup_restore "$FIXTURES/fstab" /tmp/cf_backup_test_dest

rm -rf "$FIXTURES" /mnt/system /tmp/cf_backup_test_dest
