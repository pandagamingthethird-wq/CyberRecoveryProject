#!/usr/bin/env bash
# test_e2e_install.sh — builds real fixtures (a legitimately-signed
# synthetic "Magisk-style" zip, a maliciously-signed one, certs, a
# device.conf pointing OTA_TRUSTED_CERT at the right cert) and runs
# test_e2e_install.c against them. This is the Phase 4 capstone: proves
# the full chain (ORS parse -> execute -> verify signature -> extract
# zip -> exec installer) actually works, not just its individual pieces.
set -e
cd "$(dirname "$0")/.."

WORKDIR=$(mktemp -d)
trap "rm -rf $WORKDIR; rm -f /tmp/cf_e2e_install_marker.txt" EXIT
cd "$WORKDIR"

echo "=== Generating certs ==="
openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 365 -nodes \
    -subj "/CN=Test Cert A" 2>/dev/null
openssl req -x509 -newkey rsa:2048 -keyout wrong_key.pem -out wrong_cert.pem -days 365 -nodes \
    -subj "/CN=Test Cert B (attacker)" 2>/dev/null

echo "=== Building synthetic Magisk-style installer zip ==="
mkdir -p zipcontent/META-INF/com/google/android
cat > zipcontent/META-INF/com/google/android/update-binary << 'SCRIPT'
#!/bin/sh
echo "installed-by-e2e-test api=$1 zip=$3" > /tmp/cf_e2e_install_marker.txt
exit 0
SCRIPT
chmod +x zipcontent/META-INF/com/google/android/update-binary
(cd zipcontent && zip -q -r ../unsigned.zip META-INF)

echo "=== Signing with legitimate cert ==="
python3 "$OLDPWD/src/ota/sign_ota_zip.py" unsigned.zip --cert cert.pem --key key.pem --out legit.zip > /dev/null

echo "=== Signing the SAME content with the attacker's cert ==="
python3 "$OLDPWD/src/ota/sign_ota_zip.py" unsigned.zip --cert wrong_cert.pem --key wrong_key.pem --out malicious.zip > /dev/null

echo "=== Writing device.conf fixture ==="
cat > device.conf << EOF
OTA_TRUSTED_CERT=$WORKDIR/cert.pem
EOF

echo "=== Finding libarchive (backup_engine.c now depends on it via twrp_tar.c) ==="
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
    ARCHIVE_INCLUDE="-I$OLDPWD/tests/archive_shim"; ARCHIVE_LINK="$LIBARCHIVE_PATH"
else
    echo "ERROR: no libarchive available"; exit 1
fi

echo "=== Compiling test_e2e_install ==="
gcc -Wall -Wextra -D_POSIX_C_SOURCE=200809L $ARCHIVE_INCLUDE \
    -I"$OLDPWD/src/ors" -I"$OLDPWD/src/mount" -I"$OLDPWD/src/wipe" \
    -I"$OLDPWD/src/backup" -I"$OLDPWD/src/gui" -I"$OLDPWD/src/partition" \
    -I"$OLDPWD/src/ota" -I"$OLDPWD/src/install" \
    -o cf_test_e2e_install \
    "$OLDPWD/tests/test_e2e_install.c" \
    "$OLDPWD/src/ors/openrecoveryscript.c" \
    "$OLDPWD/src/mount/mount_engine.c" \
    "$OLDPWD/src/wipe/wipe_engine.c" \
    "$OLDPWD/src/backup/backup_engine.c" \
    "$OLDPWD/src/backup/twrp_tar.c" \
    "$OLDPWD/src/backup/twrp_digest.c" \
    "$OLDPWD/src/gui/data_manager.c" \
    "$OLDPWD/src/partition/fstab_parser.c" \
    "$OLDPWD/src/ota/ota_verify.c" \
    "$OLDPWD/src/install/zip_installer.c" \
    $ARCHIVE_LINK

echo "=== Running end-to-end test ==="
rm -f /tmp/cf_e2e_install_marker.txt
./cf_test_e2e_install device.conf "$OLDPWD/tests/fixtures/fstab" legit.zip malicious.zip
