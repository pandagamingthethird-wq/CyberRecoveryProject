#!/usr/bin/env bash
# test_twrp_tar.sh — compiles and runs a real tar create/extract round
# trip against twrp_tar.c. Prefers the REAL libarchive-dev headers
# (archive.h/archive_entry.h) when available — e.g. in CI, which
# installs libarchive-dev — and only falls back to the checked-in
# minimal shim in tests/archive_shim/ (see that file's own header
# comment) when the real headers aren't present, so this test still
# runs somewhere without network access to install -dev packages.
set -e
cd "$(dirname "$0")/.."

CC=${CC:-gcc}
CFLAGS="-Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L"

# Find the real archive.h if it's installed.
REAL_HEADER_DIR=""
for dir in /usr/include /usr/local/include; do
    if [ -f "$dir/archive.h" ] && [ -f "$dir/archive_entry.h" ]; then
        REAL_HEADER_DIR="$dir"
        break
    fi
done

# Find a linkable libarchive (real .so, versioned or not).
LIBARCHIVE_PATH=""
for candidate in /usr/lib/x86_64-linux-gnu/libarchive.so /usr/lib/libarchive.so \
                  /usr/lib/x86_64-linux-gnu/libarchive.so.13 /usr/lib/libarchive.so.13; do
    if [ -f "$candidate" ]; then
        LIBARCHIVE_PATH="$candidate"
        break
    fi
done
if [ -z "$LIBARCHIVE_PATH" ]; then
    echo "SKIPPED: no libarchive runtime library found at all (not even the .so) — nothing to link against"
    exit 0
fi

if [ -n "$REAL_HEADER_DIR" ]; then
    echo "Using REAL libarchive-dev headers from $REAL_HEADER_DIR"
    INCLUDE_FLAG=""
    LINK_FLAG="-larchive"
else
    echo "WARNING: real libarchive-dev headers not found — using FALLBACK shim"
    echo "         (tests/archive_shim/) against $LIBARCHIVE_PATH directly."
    echo "         Install libarchive-dev for the authoritative test path."
    INCLUDE_FLAG="-Itests/archive_shim"
    LINK_FLAG="$LIBARCHIVE_PATH"
fi

echo "=== Building test fixture files ==="
rm -rf /tmp/tar_test_src /tmp/tar_test_dest /tmp/tar_test.tar.gz
mkdir -p /tmp/tar_test_src/subdir
echo "hello world" > /tmp/tar_test_src/file1.txt
echo "nested content" > /tmp/tar_test_src/subdir/file2.txt

echo "=== Compiling twrp_tar.c + test ==="
"$CC" $CFLAGS $INCLUDE_FLAG -Isrc/backup \
    -o /tmp/cf_test_twrp_tar \
    tests/test_twrp_tar.c src/backup/twrp_tar.c $LINK_FLAG

echo "=== Running round trip ==="
mkdir -p /tmp/tar_test_dest
/tmp/cf_test_twrp_tar
