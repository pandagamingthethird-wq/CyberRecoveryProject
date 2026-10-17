#include "ui_theme.h"
#include <stdio.h>

const cf_theme_t CF_THEME_DEFAULT = {
    .bg        = {0x0b, 0x0f, 0x14},
    .accent    = {0x00, 0xd9, 0xc0},
    .text      = {0xe8, 0xf1, 0xf5},
    .text_dim  = {0x7a, 0x8a, 0x93},
    .font_path = "/res/fonts/DejaVuSans.ttf"
};

/* NOTE: these are intentionally thin — wire them to your graphics backend
 * (minui, a raw fbdev writer, or DRM/KMS) depending on what the device
 * port provides. Kept separate from menu_actions.c so swapping rendering
 * backends never touches the action logic. */

void cf_ui_draw_menu(cf_menu_id_t menu, const cf_theme_t *theme) {
    (void)theme;
    switch (menu) {
        case CF_MENU_HOME:
            printf("[CyberFlasher] Flash | Backup | Restore | Wipe | Mount | Advanced\n");
            break;
        case CF_MENU_FLASH:
            printf("[Flash] Select image -> Select target partition -> Select size budget\n");
            break;
        case CF_MENU_SIZE_PICKER:
            printf("[Size budget] 32MB | 64MB | 128MB | Custom\n");
            break;
        case CF_MENU_TRADEOFF_PICKER:
            printf("[Still too large] Choose trade-offs to apply (or Cancel):\n");
            break;
        default:
            printf("[Menu %d]\n", menu);
    }
}

void cf_ui_draw_status_bar(const char *budget_label, int battery_pct) {
    printf("-- budget: %s | battery: %d%% --\n", budget_label, battery_pct);
}

int cf_ui_wait_for_selection(void) {
    /* Real backend: poll input events (touch/volume keys). Stub here. */
    return 0;
}
