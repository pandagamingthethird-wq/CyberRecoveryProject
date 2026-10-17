#!/usr/bin/env bash
# test_ota_verify.sh — end-to-end test of src/ota/verify_ota_zip.py against
# real generated certs and a real signed zip, including the adversarial
# cases that actually matter for a security check: tampered content, wrong
# signer, and a missing signature. This caught a real bug during
# development (see git history / the project's own notes): an earlier
# --no-chain mode ignored which cert was passed entirely and accepted ANY
# validly self-signed message, regardless of signer. Keep these cases —
# especially the "malicious" one — in any future changes to the verifier.
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
python3 "$OLDPWD/src/ota/sign_ota_zip.py" update_unsigned.zip --cert cert.pem --key key.pem --out legit_signed.zip

echo "=== Signing the SAME content with a DIFFERENT (attacker) cert ==="
python3 "$OLDPWD/src/ota/sign_ota_zip.py" update_unsigned.zip --cert wrong_cert.pem --key wrong_key.pem --out malicious_signed.zip

echo "=== Building a tampered copy of the legit zip ==="
python3 -c "
data = bytearray(open('legit_signed.zip','rb').read())
data[10] ^= 0xFF
open('tampered.zip','wb').write(bytes(data))
"

VERIFY="python3 $OLDPWD/src/ota/verify_ota_zip.py"
FAIL=0

check() {
    local desc="$1"; local expect="$2"; shift 2
    if $VERIFY "$@" > /tmp/verify_out.txt 2>&1; then actual=0; else actual=1; fi
    if [ "$actual" = "$expect" ]; then
        echo "PASS: $desc"
    else
        echo "FAIL: $desc (expected exit $expect, got $actual)"
        cat /tmp/verify_out.txt
        FAIL=1
    fi
}

check "legit zip, correct cert, chain mode"        0 legit_signed.zip --cert cert.pem
check "legit zip, correct cert, no-chain mode"      0 legit_signed.zip --cert cert.pem --no-chain
check "MALICIOUS zip (wrong signer), chain mode"    1 malicious_signed.zip --cert cert.pem
check "MALICIOUS zip (wrong signer), no-chain mode" 1 malicious_signed.zip --cert cert.pem --no-chain
check "tampered content, correct cert"              1 tampered.zip --cert cert.pem
check "unsigned zip (no footer at all)"             1 update_unsigned.zip --cert cert.pem

if [ "$FAIL" = "0" ]; then
    echo ""
    echo "=== ALL OTA VERIFIER TESTS PASSED ==="
else
    echo ""
    echo "=== OTA VERIFIER TESTS FAILED — DO NOT TRUST verify_ota_zip.py UNTIL FIXED ==="
    exit 1
fi
