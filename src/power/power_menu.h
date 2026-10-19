#ifndef CF_POWER_MENU_H
#define CF_POWER_MENU_H

#include <stdbool.h>

/*
 * power_menu — power off and reboot-to-X actions, using the real Linux
 * reboot(2) syscall with the same magic numbers and reboot-reason
 * strings Android's own init/libcutils use (LINUX_REBOOT_MAGIC1/2,
 * LINUX_REBOOT_CMD_RESTART2 + a reason string like "recovery" or
 * "bootloader"). glibc's <sys/reboot.h> wrapper only exposes the
 * no-argument reboot commands (poweroff, plain restart) — passing a
 * reason string needs the raw syscall(2), which is what this does.
 *
 * HONEST DEVICE-SPECIFIC NOTE ON "REBOOT TO EDL":
 * EDL (Emergency Download Mode) is a QUALCOMM concept (Sahara/Firehose
 * protocol). This project targets a Tecno Spark Go 2023 BF7 — MediaTek
 * MT6761. There is no EDL mode on this device. The closest MediaTek
 * equivalent is BROM/Preloader download mode (used by SP Flash Tool),
 * and unlike EDL it is NOT reliably software-triggerable across MTK
 * devices/preloader versions — the confirmed-working way to enter it is
 * the physical button combo (typically Volume Up + Power while
 * connecting USB with the device off). CF_POWER_REBOOT_DOWNLOAD below
 * attempts the common software trigger some MTK preloaders honor, but
 * this has NOT been confirmed to work on this specific BF7 unit — don't
 * rely on it; use the physical combo if the software path doesn't work.
 */

typedef enum {
    CF_POWER_OFF,
    CF_POWER_REBOOT,              /* normal reboot back into the OS */
    CF_POWER_REBOOT_RECOVERY,
    CF_POWER_REBOOT_BOOTLOADER,   /* fastboot mode — standard Android reboot reason, works the same on MTK as elsewhere */
    CF_POWER_REBOOT_DOWNLOAD      /* MTK BROM/Preloader download mode attempt — see honest note above; NOT the same thing as EDL */
} cf_power_action_t;

/*
 * The actual syscall invocation, factored out so tests can substitute a
 * fake that just records what WOULD have been called — this function
 * genuinely reboots/powers off the machine it runs on, so it must never
 * run for real inside a test.
 */
typedef int (*cf_power_syscall_fn)(int magic1, int magic2, int cmd, const char *arg);

typedef struct {
    cf_power_syscall_fn syscall_fn; /* NULL = use the real syscall (cf_power_real_syscall) */
} cf_power_options_t;

/* The real implementation — calls sync() then the raw reboot(2) syscall
 * with Android's standard magic numbers. This is what cf_power_execute
 * uses by default; exposed so cf_power_options_t can reference it or a
 * test can distinguish "real" from "faked". DO NOT call this directly
 * in a test. */
int cf_power_real_syscall(int magic1, int magic2, int cmd, const char *arg);

/*
 * Executes `action`. Calls sync() first (flush filesystems before
 * reboot — skipping this risks data loss, same reason Android's own
 * reboot path always does it), then invokes the syscall (real or
 * injected via options->syscall_fn). Returns false if the syscall
 * itself reports failure (on a real device this function normally never
 * returns at all on success, since the machine reboots) — the return
 * value mainly matters for the injected-syscall test path.
 */
bool cf_power_execute(cf_power_action_t action, const cf_power_options_t *options);

/* Human-readable label for a power menu entry (for wiring into a theme
 * page's "text" attribute / for logging). */
const char *cf_power_action_label(cf_power_action_t action);

#endif /* CF_POWER_MENU_H */
