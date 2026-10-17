#!/usr/bin/env bash
# run_tests.sh — compiles and runs every standalone test in this folder
# against the corresponding real source file (not a mock). Every test
# here was actually run during development, not just written —
# including the OTA zip signature verifier, which was tested against a
# real self-signed cert through an actual attack scenario (see
# src/ota/verify_ota_zip.py's own header comment and
# tests/test_ota_verify.sh for that one specifically, since it's a
# Python/shell test rather than a C unit test).
#
# NOT covered here: twrp_tar.c (needs libarchive-dev, which CI installs
# but this script doesn't assume is present — see the project README).
set -e
cd "$(dirname "$0")/.."

CC=${CC:-gcc}
CFLAGS="-Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L"

run_test() {
    local name="$1"; shift
    local test_src="tests/$1"; shift
    local extra_srcs=("$@")
    echo "=== $name ==="
    local include_dirs=""
    for src in "${extra_srcs[@]}"; do
        include_dirs="$include_dirs -I$(dirname "$src")"
    done
    "$CC" $CFLAGS -Itests $include_dirs -o "/tmp/cf_test_$name" "$test_src" "${extra_srcs[@]}" -lpthread
    "/tmp/cf_test_$name"
    echo ""
}

run_test data_manager      test_data_manager.c      src/gui/data_manager.c
run_test theme_xml         test_theme_xml.c         src/gui/theme_xml.c
run_test input_backend     test_input_backend.c     src/graphics/input_backend.c
run_test fstab_parser      test_fstab_parser.c      src/partition/fstab_parser.c
run_test twrp_digest       test_twrp_digest.c       src/backup/twrp_digest.c

echo "=== wipe_engine ==="
"$CC" $CFLAGS -Isrc/wipe -Isrc/partition -o /tmp/cf_test_wipe_engine \
    tests/test_wipe_engine.c src/wipe/wipe_engine.c src/partition/fstab_parser.c
PATH="$(pwd)/tests/fake_bin:$PATH" /tmp/cf_test_wipe_engine
echo ""
run_test openrecoveryscript test_openrecoveryscript.c src/ors/openrecoveryscript.c \
    src/mount/mount_engine.c src/wipe/wipe_engine.c src/backup/backup_engine.c \
    src/gui/data_manager.c src/partition/fstab_parser.c
run_test sideload_bridge   test_sideload_bridge.c   src/sideload/sideload_bridge.c

echo "=== ors_execute (integration — real mount_engine/wipe_engine/backup_engine/data_manager) ==="
"$CC" $CFLAGS -Isrc/ors -Isrc/mount -Isrc/wipe -Isrc/backup -Isrc/gui -Isrc/partition \
    -o /tmp/cf_test_ors_execute \
    tests/test_ors_execute.c \
    src/ors/openrecoveryscript.c \
    src/mount/mount_engine.c \
    src/wipe/wipe_engine.c \
    src/backup/backup_engine.c \
    src/gui/data_manager.c \
    src/partition/fstab_parser.c
# tests/fake_bin/ ships fake mount/umount/mkfs.ext4/mkfs.f2fs so this
# exercises the real engines' command construction without needing root
# or a real block device — put it FIRST on PATH so these are found
# before any real system binaries of the same name.
rm -f /tmp/cf_engine_calls.log
PATH="$(pwd)/tests/fake_bin:$PATH" /tmp/cf_test_ors_execute
echo "--- captured mount/umount/mkfs invocations ---"
cat /tmp/cf_engine_calls.log
echo ""

echo "=== All C unit tests passed ==="
echo "(OTA zip verifier has its own test: tests/test_ota_verify.sh)"
