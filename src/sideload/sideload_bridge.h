#ifndef CF_SIDELOAD_BRIDGE_H
#define CF_SIDELOAD_BRIDGE_H

#include <stdbool.h>

/*
 * sideload_bridge — NOT a reimplementation of TWRP's twrpadbd. A custom
 * ADB daemon means reimplementing the USB FunctionFS gadget protocol and
 * the ADB wire protocol (AUTH, OPEN/WRTE/OKAY/CLSE frames, the sideload
 * host-side "adb sideload" handshake) — that's a project of its own and
 * is explicitly out of scope here; attempting a partial/wrong version of
 * a protocol like this is worse than not having it, since a subtly broken
 * implementation fails in confusing ways rather than just not existing.
 *
 * What this DOES do: the same receiving-side mechanism TWRP's twrpadbd
 * uses internally once the ADB protocol handshake is already done — the
 * sideloaded file gets written to a FIFO (named pipe) by whatever process
 * handles the ADB transport, and this reads it from there into a real
 * file. In a real build, that means launching AOSP's actual prebuilt
 * `adbd` binary with `--sideload <fifo-path>` (the same binary and
 * invocation TWRP itself documents using, rather than a custom one) and
 * pointing it at the FIFO this code reads from.
 */

/* Blocks reading from `fifo_path` until EOF, writing everything received
 * to `output_path`. Returns the number of bytes written, or -1 on error.
 * Caller is responsible for having something (adbd --sideload, or a test
 * harness) actually writing to the FIFO on the other end. */
long cf_sideload_receive(const char *fifo_path, const char *output_path);

/* Creates the FIFO at `fifo_path` if it doesn't already exist (mkfifo).
 * Returns true on success or if it already exists as a FIFO. */
bool cf_sideload_prepare_fifo(const char *fifo_path);

#endif /* CF_SIDELOAD_BRIDGE_H */
