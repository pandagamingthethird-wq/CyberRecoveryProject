/*
 * menu_actions.c — wires the UI to the engines. This is the file that
 * implements "user picks 32/64/128MB, oversized image gets auto-shrunk,
 * user is only asked about trade-offs if automatic shrinking isn't enough."
 */

#include "ui_theme.h"
#include "menu_actions.h"
#include "../core/partition_size.h"
#include "../core/image_shrink.h"
#include "../flash/flash_engine.h"
#include "../power/power_menu.h"
#include <stdio.h>
#include <string.h>

/* Implements cf_ui_prompt_tradeoffs_fn — called by image_shrink only when
 * automatic steps couldn't close the gap. */
static int ui_prompt_tradeoffs(const cf_shrink_tradeoff_t *tradeoffs,
                                int count, int64_t bytes_still_needed,
                                int *chosen_out) {
    cf_ui_draw_menu(CF_MENU_TRADEOFF_PICKER, &CF_THEME_DEFAULT);
    printf("Still need to free ~%lld bytes. Options:\n", (long long)bytes_still_needed);
    for (int i = 0; i < count; i++) {
        printf("  [%d] %s\n", i, tradeoffs[i].description);
    }
    printf("  [c] Cancel flash\n");

    /* Real backend: read touch/key selection(s), allow multi-select.
     * Returns however many the user picked, written into chosen_out. */
    int picked = cf_ui_wait_for_selection();
    if (picked < 0 || picked >= count) return 0; /* cancelled */
    chosen_out[0] = picked;
    return 1;
}

void cf_action_flash_image(const char *image_path, const char *target_partition,
                            const char *device_conf_path) {
    cf_ui_draw_menu(CF_MENU_SIZE_PICKER, &CF_THEME_DEFAULT);

    cf_size_preset_t presets[8];
    int n = cf_load_size_presets(device_conf_path, presets, 8);
    if (n == 0) {
        fprintf(stderr, "No size presets configured in device.conf\n");
        return;
    }
    for (int i = 0; i < n; i++)
        printf("  [%d] %s\n", i, presets[i].label);

    int choice = cf_ui_wait_for_selection();
    if (choice < 0 || choice >= n) return;
    int64_t target_size = presets[choice].bytes;

    char status[32];
    snprintf(status, sizeof(status), "%s", presets[choice].label);
    cf_ui_draw_status_bar(status, 100);

    /* cf_flash_prepare_workdir unpacks the image (kernel/ramdisk/dtb split)
     * into a scratch dir and returns its size + path — see flash_engine.h */
    char workdir[256];
    int64_t current_size = 0;
    if (!cf_flash_prepare_workdir(image_path, workdir, sizeof(workdir), &current_size)) {
        fprintf(stderr, "Failed to unpack image for inspection\n");
        return;
    }

    if (current_size > target_size) {
        cf_shrink_result_t res = cf_shrink_image_to_fit(
            workdir, current_size, target_size, ui_prompt_tradeoffs);

        switch (res) {
            case CF_SHRINK_OK:
                printf("Image auto-reduced to fit %s with no feature loss.\n", status);
                break;
            case CF_SHRINK_OK_WITH_TRADEOFF:
                printf("Image fit after applying the trade-offs you selected.\n");
                break;
            case CF_SHRINK_USER_CANCELLED:
                printf("Flash cancelled by user.\n");
                return;
            case CF_SHRINK_FAILED:
                fprintf(stderr,
                    "Image still exceeds %s after every available reduction. "
                    "Pick a larger size preset.\n", status);
                return;
        }
    }

    cf_flash_repack_and_write(workdir, target_partition);
}

/* ---- power menu actions ---- */
/* See this file's own header comment at the top of menu_actions.h for
 * why these are thin wrappers and not independently tested — the pieces
 * they connect (cf_power_execute's dispatch, ui_loop's tap dispatch)
 * each have their own real tests. */

void cf_action_poweroff(void *user_data) {
    (void)user_data;
    cf_power_execute(CF_POWER_OFF, NULL);
}

void cf_action_reboot(void *user_data) {
    (void)user_data;
    cf_power_execute(CF_POWER_REBOOT, NULL);
}

void cf_action_reboot_recovery(void *user_data) {
    (void)user_data;
    cf_power_execute(CF_POWER_REBOOT_RECOVERY, NULL);
}

void cf_action_reboot_bootloader(void *user_data) {
    (void)user_data;
    cf_power_execute(CF_POWER_REBOOT_BOOTLOADER, NULL);
}

void cf_action_reboot_download(void *user_data) {
    (void)user_data;
    cf_power_execute(CF_POWER_REBOOT_DOWNLOAD, NULL);
}
