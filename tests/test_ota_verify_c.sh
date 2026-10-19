#!/usr/bin/env bash
# test_ota_verify_c.sh — builds the same adversarial fixtures as
# test_ota_verify.sh (legit/malicious/tampered/unsigned zips, two
# different self-signed certs) and runs them through the C port
# (ota_verify.c) instead of the Python version, proving the port carries
# the same security properties — especially the malicious-signer
# rejection, which is the exact case an earlier version of this logic
# got wrong.
set -e
cd "$(dirname "$0")/.."

WORKDIR=$(mktemp -d)
trap "rm -rf $WORKDIR" EXIT
cd "$WORKDIR"

echo "=== Generating test certs ==="
openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 365 -nodes \
    -subj "/CN=Test Cert A" 2>/dev/null
openssl req -x509 -newkey rsa:2048 -keyout wrong_key.pem -out wrong_cert.pem -days 365 -nodes \
    -subj "/CN=Test Cert B (attacker)" 2>/dev/null

echo "=== Building test zip ==="
python3 -c "
import zipfile
with zipfile.ZipFile('update_unsigned.zip', 'w') as z:
    z.writestr('META-INF/com/google/android/update-binary', 'fake updater')
    z.writestr('system/build.prop', 'ro.build.version=test')
"

echo "=== Signing with legitimate cert ==="
python3 "$OLDPWD/src/ota/sign_ota_zip.py" update_unsigned.zip --cert cert.pem --key key.pem --out legit_signed.zip > /dev/null

echo "=== Signing the SAME content with a DIFFERENT (attacker) cert ==="
python3 "$OLDPWD/src/ota/sign_ota_zip.py" update_unsigned.zip --cert wrong_cert.pem --key wrong_key.pem --out malicious_signed.zip > /dev/null

echo "=== Building a tampered copy of the legit zip ==="
python3 -c "
data = bytearray(open('legit_signed.zip','rb').read())
data[10] ^= 0xFF
open('tampered.zip','wb').write(bytes(data))
"

echo "=== Compiling C verifier test ==="
gcc -Wall -Wextra -D_POSIX_C_SOURCE=200809L -I"$OLDPWD/src/ota" \
    -o cf_test_ota_verify_c "$OLDPWD/tests/test_ota_verify_c.c" "$OLDPWD/src/ota/ota_verify.c"

echo "=== Running C verifier against all 6 scenarios ==="
./cf_test_ota_verify_c legit_signed.zip malicious_signed.zip tampered.zip update_unsigned.zip cert.pem unused
