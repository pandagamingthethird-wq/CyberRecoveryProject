#define _DEFAULT_SOURCE /* for syscall(2) and sync() visibility under -std=c11 */
#include "power_menu.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

/* Standard Linux kernel reboot(2) ABI constants (linux/reboot.h) —
 * stable across the kernel's entire history, same values Android's
 * libcutils android_reboot() uses. glibc's <sys/reboot.h> wrapper
 * doesn't expose RESTART2 with a reason-string argument, so this goes
 * through the raw syscall(2) directly, same as Android itself does. */
#define LINUX_REBOOT_MAGIC1      0xfee1dead
#define LINUX_REBOOT_MAGIC2      0x28121969
#define LINUX_REBOOT_CMD_RESTART    0x01234567
#define LINUX_REBOOT_CMD_POWER_OFF  0x4321FEDC
#define LINUX_REBOOT_CMD_RESTART2   0xA1B2C3D4

int cf_power_real_syscall(int magic1, int magic2, int cmd, const char *arg) {
    return (int)syscall(SYS_reboot, magic1, magic2, cmd, arg);
}

const char *cf_power_action_label(cf_power_action_t action) {
    switch (action) {
        case CF_POWER_OFF:                return "Power Off";
        case CF_POWER_REBOOT:             return "Reboot";
        case CF_POWER_REBOOT_RECOVERY:    return "Reboot to Recovery";
        case CF_POWER_REBOOT_BOOTLOADER:  return "Reboot to Fastboot";
        case CF_POWER_REBOOT_DOWNLOAD:    return "Reboot to Download Mode";
        default:                          return "Unknown";
    }
}

bool cf_power_execute(cf_power_action_t action, const cf_power_options_t *options) {
    cf_power_syscall_fn do_syscall = cf_power_real_syscall;
    if (options && options->syscall_fn) do_syscall = options->syscall_fn;

    sync(); /* flush filesystems before any reboot/poweroff path below —
              * skipping this risks data loss, same as Android's own
              * reboot path. */

    int rc;
    switch (action) {
        case CF_POWER_OFF:
            printf("power_menu: powering off\n");
            rc = do_syscall(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
                             LINUX_REBOOT_CMD_POWER_OFF, NULL);
            break;

        case CF_POWER_REBOOT:
            printf("power_menu: rebooting\n");
            rc = do_syscall(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
                             LINUX_REBOOT_CMD_RESTART, NULL);
            break;

        case CF_POWER_REBOOT_RECOVERY:
            printf("power_menu: rebooting to recovery\n");
            rc = do_syscall(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
                             LINUX_REBOOT_CMD_RESTART2, "recovery");
            break;

        case CF_POWER_REBOOT_BOOTLOADER:
            printf("power_menu: rebooting to bootloader/fastboot\n");
            rc = do_syscall(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
                             LINUX_REBOOT_CMD_RESTART2, "bootloader");
            break;

        case CF_POWER_REBOOT_DOWNLOAD:
            /* See power_menu.h's honest note: this is MTK BROM/Preloader
             * download mode, NOT Qualcomm EDL, and this software trigger
             * is unconfirmed on BF7 specifically — the physical button
             * combo is the known-working fallback. */
            printf("power_menu: attempting reboot to MTK download mode "
                   "(NOT EDL — unconfirmed on this device, see power_menu.h)\n");
            rc = do_syscall(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
                             LINUX_REBOOT_CMD_RESTART2, "download");
            break;

        default:
            fprintf(stderr, "power_menu: unknown action\n");
            return false;
    }

    if (rc != 0) {
        fprintf(stderr, "power_menu: reboot syscall failed\n");
        return false;
    }
    return true;
}
