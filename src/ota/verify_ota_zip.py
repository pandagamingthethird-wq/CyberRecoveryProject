#!/usr/bin/env python3
"""
verify_ota_zip.py — verifies an OTA-style signed zip using the same
whole-file signature scheme AOSP's recovery verifier (verifier.cpp) and
TWRP use: the file is a normal zip, followed by a trailer appended INSIDE
the zip's own EOCD comment field:

    [normal zip data][EOCD record][comment = SIG_DER + sig_size(2B LE) + 0xFFFF]

The signed data is the entire file EXCLUDING the comment bytes (i.e.
everything through the end of the EOCD record). This is why OTA zips can
be "signed" after the fact without re-zipping: the signature lives in a
field zip readers already treat as free-form trailing data.

WHY THIS IS A PYTHON SCRIPT, NOT C WIRED INTO THE RECOVERY BINARY:
Linking OpenSSL into the actual arm64 Android recovery binary needs a
proper cross-compiled libcrypto from the NDK, which isn't set up as part
of this project yet (see README's open items). This script runs host-side
— as a CI gate before a zip is ever allowed into a release, or on your
workstation before you sideload something — using the system `openssl`
command via subprocess, which this sandbox actually has available, so
this is the one piece of the OTA story I could build AND test for real
rather than writing untested crypto code.

Verified during development against:
  - A validly signed test zip -> passes
  - The same zip with a single byte flipped in its content -> correctly
    rejected
  - A zip signed with a DIFFERENT cert than the one passed via --cert ->
    correctly rejected (tests real chain/signer validation, not just
    "a signature exists")

Usage:
    python3 verify_ota_zip.py signed.zip --cert /path/to/trusted_cert.pem
"""

import argparse
import struct
import subprocess
import sys
import tempfile
import os

EOCD_SIGNATURE = b'\x50\x4b\x05\x06'
FOOTER_MAGIC = b'\xff\xff'


def find_eocd(data: bytes) -> int:
    """Scans backward for the End Of Central Directory signature.
    Zip comments are capped at 65535 bytes, so the EOCD can't be further
    than ~65557 bytes from the end of a well-formed file."""
    search_start = max(0, len(data) - 65557)
    pos = data.rfind(EOCD_SIGNATURE, search_start)
    if pos == -1:
        raise ValueError("Not a valid zip: EOCD record not found")
    return pos


def extract_signature(data: bytes):
    """Returns (signed_data, sig_der) or raises ValueError if the file
    isn't signed in the expected format."""
    eocd_pos = find_eocd(data)
    comment_len = struct.unpack('<H', data[eocd_pos + 20:eocd_pos + 22])[0]
    comment = data[eocd_pos + 22: eocd_pos + 22 + comment_len]

    if len(comment) < 4:
        raise ValueError("No signature footer present (comment too short)")

    magic = comment[-2:]
    if magic != FOOTER_MAGIC:
        raise ValueError(f"No signature footer present (expected magic "
                          f"{FOOTER_MAGIC.hex()}, got {magic.hex()}) — "
                          f"this zip is not signed, or uses a different "
                          f"signing scheme")

    sig_size = struct.unpack('<H', comment[-4:-2])[0]
    if sig_size > len(comment) - 4:
        raise ValueError("Malformed footer: declared signature size exceeds comment length")

    sig_der = comment[-4 - sig_size: -4]
    signed_data = data[0: eocd_pos + 22]  # everything except the comment
    return signed_data, sig_der


def get_cert_fingerprint(cert_pem_path: str) -> str:
    r = subprocess.run(["openssl", "x509", "-in", cert_pem_path, "-noout", "-fingerprint", "-sha256"],
                        capture_output=True, text=True)
    if r.returncode != 0:
        raise ValueError(f"Could not read fingerprint of {cert_pem_path}: {r.stderr}")
    return r.stdout.strip()


def get_embedded_signer_fingerprint(sig_der_path: str) -> str:
    """Extracts the signer cert embedded IN the PKCS7 blob and fingerprints
    it. This is the check that was MISSING before — -noverify checks a
    signature is self-consistent with whatever cert is embedded in the
    message, but says nothing about whether that's the cert you actually
    trust. Pinning by fingerprint here is what makes --no-chain mode
    actually mean something instead of silently accepting any signer."""
    extract = subprocess.run(
        ["openssl", "pkcs7", "-inform", "DER", "-in", sig_der_path, "-print_certs"],
        capture_output=True, text=True)
    if extract.returncode != 0 or not extract.stdout.strip():
        raise ValueError("Could not extract signer certificate from PKCS7 blob")

    with tempfile.NamedTemporaryFile(mode="w", suffix=".pem", delete=False) as f:
        f.write(extract.stdout)
        embedded_cert_path = f.name
    try:
        return get_cert_fingerprint(embedded_cert_path)
    finally:
        os.unlink(embedded_cert_path)


def verify_signature(signed_data: bytes, sig_der: bytes, cert_path: str, require_chain: bool) -> bool:
    with tempfile.TemporaryDirectory() as tmp:
        data_path = os.path.join(tmp, "signed_data.bin")
        sig_path = os.path.join(tmp, "sig.der")
        with open(data_path, "wb") as f:
            f.write(signed_data)
        with open(sig_path, "wb") as f:
            f.write(sig_der)

        # Fingerprint pin — ALWAYS enforced, both modes. Without this,
        # -noverify mode checks a signature is cryptographically
        # self-consistent with whatever cert happens to be embedded in
        # the message, which says nothing about whether that's a cert
        # you actually trust. An earlier version of this script skipped
        # this check in --no-chain mode and incorrectly accepted a zip
        # signed with a completely different, untrusted key — caught by
        # testing against an actual adversarial case before shipping.
        try:
            expected_fp = get_cert_fingerprint(cert_path)
            actual_fp = get_embedded_signer_fingerprint(sig_path)
        except ValueError as e:
            print(f"Fingerprint check failed: {e}", file=sys.stderr)
            return False

        if expected_fp != actual_fp:
            print(f"REJECTED: signer certificate does not match expected cert.\n"
                  f"  expected: {expected_fp}\n"
                  f"  actual:   {actual_fp}", file=sys.stderr)
            return False

        cmd = ["openssl", "smime", "-verify",
               "-in", sig_path, "-inform", "DER",
               "-content", data_path,
               "-out", os.devnull]
        if require_chain:
            cmd += ["-CAfile", cert_path]
        else:
            # Signature math + fingerprint pin only — no CA chain check.
            # Fine for a self-signed cert you're pinning directly (the
            # fingerprint check above is what makes this safe); if you
            # want real CA chain validation against an intermediate/root
            # setup, don't pass --no-chain.
            cmd += ["-noverify", "-certfile", cert_path]

        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"openssl verify failed:\n{result.stderr}", file=sys.stderr)
            return False
        return True


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("zip_path")
    ap.add_argument("--cert", required=True,
                     help="Trusted certificate (PEM) to verify against")
    ap.add_argument("--no-chain", action="store_true",
                     help="Skip CA chain validation (TESTING ONLY — "
                          "accepts any cert whose signature is "
                          "cryptographically self-consistent, not "
                          "necessarily one you trust)")
    args = ap.parse_args()

    with open(args.zip_path, "rb") as f:
        data = f.read()

    try:
        signed_data, sig_der = extract_signature(data)
    except ValueError as e:
        print(f"REJECTED: {e}", file=sys.stderr)
        sys.exit(1)

    print(f"Found signature footer: {len(sig_der)} bytes, "
          f"covering {len(signed_data)} bytes of signed data "
          f"({len(data) - len(signed_data)} bytes of comment/footer excluded)")

    ok = verify_signature(signed_data, sig_der, args.cert, require_chain=not args.no_chain)
    if ok:
        print("✅ VERIFIED — signature is valid for the provided cert")
        sys.exit(0)
    else:
        print("❌ REJECTED — signature verification failed", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
