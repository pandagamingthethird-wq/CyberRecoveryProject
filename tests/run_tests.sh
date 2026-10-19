#!/usr/bin/env bash
# run_tests.sh — compiles and runs every standalone test in this folder
# against the corresponding real source file (not a mock). Every test
# here was actually run during development, not just written.
#
# backup_engine.c now depends on twrp_tar.c (libarchive) for system/
# vendor file-level backup, so anything linking backup_engine.c needs
# libarchive too. This script prefers the REAL libarchive-dev headers
# when installed (as in CI) and falls back to tests/archive_shim/ (see
# that file's own header comment) otherwise, same logic as
# test_twrp_tar.sh.
set -e
cd "$(dirname "$0")/.."

CC=${CC:-gcc}
CFLAGS="-Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L"

REAL_HEADER_DIR=""
for dir in /usr/include /usr/local/include; do
    if [ -f "$dir/archive.h" ] && [ -f "$dir/archive_entry.h" ]; then
        REAL_HEADER_DIR="$dir"
        break
    fi
done
LIBARCHIVE_PATH=""
for candidate in /usr/lib/x86_64-linux-gnu/libarchive.so /usr/lib/libarchive.so \
                  /usr/lib/x86_64-linux-gnu/libarchive.so.13 /usr/lib/libarchive.so.13; do
    if [ -f "$candidate" ]; then LIBARCHIVE_PATH="$candidate"; break; fi
done

if [ -n "$REAL_HEADER_DIR" ]; then
    ARCHIVE_INCLUDE=""
    ARCHIVE_LINK="-larchive"
elif [ -n "$LIBARCHIVE_PATH" ]; then
    echo "NOTE: using FALLBACK archive_shim headers (real libarchive-dev not found)"
    ARCHIVE_INCLUDE="-Itests/archive_shim"
    ARCHIVE_LINK="$LIBARCHIVE_PATH"
else
    echo "ERROR: no libarchive (real headers or even the .so) found — cannot build backup_engine.c-dependent tests"
    exit 1
fi

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

echo "=== fb_text (font rendering) ==="
"$CC" $CFLAGS -Isrc/graphics -o /tmp/cf_test_fb_text \
    tests/test_fb_text.c src/graphics/fb_backend.c src/graphics/font5x7.c
/tmp/cf_test_fb_text
echo ""

echo "=== ui_loop (integration — theme_xml + fb_backend + font5x7 + input_backend wired together) ==="
"$CC" $CFLAGS -Isrc/ui -Isrc/gui -Isrc/graphics -o /tmp/cf_test_ui_loop \
    tests/test_ui_loop.c src/ui/ui_loop.c src/gui/theme_xml.c \
    src/graphics/fb_backend.c src/graphics/font5x7.c src/graphics/input_backend.c
/tmp/cf_test_ui_loop
echo ""

run_test input_backend     test_input_backend.c     src/graphics/input_backend.c
run_test fstab_parser      test_fstab_parser.c      src/partition/fstab_parser.c
run_test twrp_digest       test_twrp_digest.c       src/backup/twrp_digest.c

echo "=== twrp_tar (real libarchive round trip) ==="
bash tests/test_twrp_tar.sh
echo ""

echo "=== wipe_engine ==="
"$CC" $CFLAGS -Isrc/wipe -Isrc/partition -o /tmp/cf_test_wipe_engine \
    tests/test_wipe_engine.c src/wipe/wipe_engine.c src/partition/fstab_parser.c
PATH="$(pwd)/tests/fake_bin:$PATH" /tmp/cf_test_wipe_engine
echo ""

echo "=== openrecoveryscript ==="
"$CC" $CFLAGS $ARCHIVE_INCLUDE -Isrc/ors -Isrc/mount -Isrc/wipe -Isrc/backup -Isrc/gui \
    -Isrc/partition -Isrc/ota -Isrc/install \
    -o /tmp/cf_test_openrecoveryscript \
    tests/test_openrecoveryscript.c \
    src/ors/openrecoveryscript.c src/mount/mount_engine.c src/wipe/wipe_engine.c \
    src/backup/backup_engine.c src/backup/twrp_tar.c src/backup/twrp_digest.c \
    src/gui/data_manager.c src/partition/fstab_parser.c \
    src/ota/ota_verify.c src/install/zip_installer.c \
    $ARCHIVE_LINK
/tmp/cf_test_openrecoveryscript
echo ""

run_test sideload_bridge   test_sideload_bridge.c   src/sideload/sideload_bridge.c

echo "=== ors_execute (integration — real mount_engine/wipe_engine/backup_engine/data_manager) ==="
"$CC" $CFLAGS $ARCHIVE_INCLUDE -Isrc/ors -Isrc/mount -Isrc/wipe -Isrc/backup -Isrc/gui \
    -Isrc/partition -Isrc/ota -Isrc/install \
    -o /tmp/cf_test_ors_execute \
    tests/test_ors_execute.c \
    src/ors/openrecoveryscript.c \
    src/mount/mount_engine.c \
    src/wipe/wipe_engine.c \
    src/backup/backup_engine.c \
    src/backup/twrp_tar.c \
    src/backup/twrp_digest.c \
    src/gui/data_manager.c \
    src/partition/fstab_parser.c \
    src/ota/ota_verify.c \
    src/install/zip_installer.c \
    $ARCHIVE_LINK
# tests/fake_bin/ ships fake mount/umount/mkfs.ext4/mkfs.f2fs so this
# exercises the real engines' command construction without needing root
# or a real block device — put it FIRST on PATH so these are found
# before any real system binaries of the same name.
rm -f /tmp/cf_engine_calls.log
PATH="$(pwd)/tests/fake_bin:$PATH" /tmp/cf_test_ors_execute
echo "--- captured mount/umount/mkfs invocations ---"
cat /tmp/cf_engine_calls.log
echo ""

echo "=== power_menu (dispatch logic only — real syscall never invoked) ==="
"$CC" $CFLAGS -Isrc/power -o /tmp/cf_test_power_menu \
    tests/test_power_menu.c src/power/power_menu.c
/tmp/cf_test_power_menu
echo ""

echo "=== zip_installer ==="
"$CC" $CFLAGS -Isrc/install -o /tmp/cf_test_zip_installer \
    tests/test_zip_installer.c src/install/zip_installer.c
/tmp/cf_test_zip_installer
echo ""

echo "=== backup_restore (real dd + tar round trip against synthetic partitions) ==="
bash tests/test_backup_restore.sh
echo ""

echo "=== All C unit tests passed ==="
echo "(OTA zip verifier (Python): tests/test_ota_verify.sh)"
echo "(OTA zip verifier (C port): tests/test_ota_verify_c.sh)"
echo "(End-to-end install capstone test: tests/test_e2e_install.sh)"
