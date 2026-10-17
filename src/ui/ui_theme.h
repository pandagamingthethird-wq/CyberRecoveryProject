#ifndef CF_UI_THEME_H
#define CF_UI_THEME_H

/*
 * CyberFlasher's UI is deliberately NOT a TWRP skin: dark background,
 * single accent color, flat full-width list rows instead of TWRP's
 * boxed-button grid, and a persistent top status bar (partition size
 * budget + battery) instead of TWRP's page-based navigation.
 *
 * This header defines the theme + menu contract; the actual pixel
 * rendering depends on whichever graphics backend the device port uses
 * (minui/fbdev/DRM) — swap cf_gfx_* for your backend's calls.
 */

typedef struct {
    unsigned char bg[3];        /* near-black background */
    unsigned char accent[3];    /* single accent color, e.g. cyan */
    unsigned char text[3];
    unsigned char text_dim[3];  /* secondary/disabled text */
    const char *font_path;
} cf_theme_t;

typedef enum {
    CF_MENU_HOME,
    CF_MENU_FLASH,
    CF_MENU_BACKUP,
    CF_MENU_RESTORE,
    CF_MENU_WIPE,
    CF_MENU_MOUNT,
    CF_MENU_ADVANCED,
    CF_MENU_SIZE_PICKER,     /* 32/64/128MB/Custom chooser */
    CF_MENU_TRADEOFF_PICKER  /* "remove extra languages?" style prompt */
} cf_menu_id_t;

/* Default theme — projects can ship alternates and switch at runtime */
extern const cf_theme_t CF_THEME_DEFAULT;

void cf_ui_draw_menu(cf_menu_id_t menu, const cf_theme_t *theme);
void cf_ui_draw_status_bar(const char *budget_label, int battery_pct);
int  cf_ui_wait_for_selection(void); /* returns selected item index */

#endif /* CF_UI_THEME_H */
