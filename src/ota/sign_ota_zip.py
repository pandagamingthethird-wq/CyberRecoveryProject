#!/usr/bin/env python3
"""
sign_ota_zip.py — reference signer matching verify_ota_zip.py's scheme.
Not part of the shipped project (signing happens on a build server with
the real private key, not on-device) — this exists to produce correctly
signed test fixtures and to document the correct two-pass algorithm,
since the naive "sign then append" approach is subtly wrong (see the
comment below).

THE BUG THIS AVOIDS: the signed data includes the zip's EOCD comment-
length field. That field's value depends on the footer size, which
depends on the signature size — so if you sign the file BEFORE setting
the final comment-length, you sign one value and verify against another.
For RSA + a fixed key/cert, the PKCS7 DER signature size is deterministic
regardless of message content, so: predict the size first (sign a throwaway
message), patch the comment-length field to the final value, THEN sign the
now-correctly-patched file, and verify the resulting signature is exactly
the predicted size before trusting it.
"""
import argparse
import struct
import subprocess
import sys
import tempfile
import os

FOOTER_MAGIC = b'\xff\xff'


def openssl_sign(data_path: str, cert: str, key: str, out_path: str):
    cmd = ["openssl", "smime", "-sign", "-signer", cert, "-inkey", key,
           "-in", data_path, "-outform", "DER", "-binary", "-noattr",
           "-out", out_path]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stderr, file=sys.stderr)
        sys.exit(1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("zip_path")
    ap.add_argument("--cert", required=True)
    ap.add_argument("--key", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    with open(args.zip_path, "rb") as f:
        unsigned = f.read()

    if unsigned[-2:] != b'\x00\x00':
        print("WARNING: input zip already has a non-empty comment; "
              "this signer assumes an unsigned zip with comment_length=0.",
              file=sys.stderr)

    with tempfile.TemporaryDirectory() as tmp:
        # Pass 1: predict signature size with a throwaway signature over
        # the unpatched file (content doesn't matter for size, only key+cert do)
        predict_sig_path = os.path.join(tmp, "predict_sig.der")
        predict_input = os.path.join(tmp, "predict_input.bin")
        with open(predict_input, "wb") as f:
            f.write(unsigned)
        openssl_sign(predict_input, args.cert, args.key, predict_sig_path)
        predicted_sig_size = os.path.getsize(predict_sig_path)
        predicted_footer_size = predicted_sig_size + 4
        print(f"Predicted signature size: {predicted_sig_size} bytes "
              f"(footer: {predicted_footer_size} bytes)")

        # Pass 2: patch the comment-length field to the FINAL value, THEN sign
        patched = bytearray(unsigned)
        patched[-2:] = struct.pack('<H', predicted_footer_size)
        patched_path = os.path.join(tmp, "patched.bin")
        with open(patched_path, "wb") as f:
            f.write(patched)

        real_sig_path = os.path.join(tmp, "real_sig.der")
        openssl_sign(patched_path, args.cert, args.key, real_sig_path)

        with open(real_sig_path, "rb") as f:
            sig_der = f.read()

        if len(sig_der) != predicted_sig_size:
            print(f"ERROR: predicted size {predicted_sig_size} but actual "
                  f"signature is {len(sig_der)} bytes — the deterministic-size "
                  f"assumption didn't hold for this key/cert. Aborting rather "
                  f"than producing a mismatched file.", file=sys.stderr)
            sys.exit(1)

        footer = sig_der + struct.pack('<H', len(sig_der)) + FOOTER_MAGIC
        final = bytes(patched) + footer

    with open(args.out, "wb") as f:
        f.write(final)
    print(f"Wrote signed zip: {args.out} ({len(final)} bytes)")


if __name__ == "__main__":
    main()
