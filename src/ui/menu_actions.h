#ifndef CF_MENU_ACTIONS_H
#define CF_MENU_ACTIONS_H

/*
 * menu_actions — the action callbacks a theme's buttons dispatch to via
 * cf_ui_run_page()'s binding table (ui_loop.h). Each function here
 * matches that table's `void (*callback)(void *user_data)` signature.
 *
 * Example theme page wiring these up (see theme_xml.h for the XML
 * subset this parses):
 *   <page name="power">
 *     <object type="button" x="10" y="10"  w="120" h="24" text="Reboot" action="reboot"/>
 *     <object type="button" x="10" y="40"  w="120" h="24" text="Recovery" action="reboot_recovery"/>
 *     <object type="button" x="10" y="70"  w="120" h="24" text="Fastboot" action="reboot_bootloader"/>
 *     <object type="button" x="10" y="100" w="120" h="24" text="Download" action="reboot_download"/>
 *     <object type="button" x="10" y="130" w="120" h="24" text="Power Off" action="poweroff"/>
 *   </page>
 * ...and a cf_ui_action_binding_t table:
 *   { "reboot",            cf_action_reboot },
 *   { "reboot_recovery",   cf_action_reboot_recovery },
 *   { "reboot_bootloader", cf_action_reboot_bootloader },
 *   { "reboot_download",   cf_action_reboot_download },
 *   { "poweroff",          cf_action_poweroff },
 *
 * NOTE ON TEST COVERAGE: these wrapper functions are thin — each just
 * calls cf_power_execute() with the matching action enum and no
 * options (meaning: use the real reboot(2) syscall). cf_power_execute's
 * own dispatch logic (right magic numbers/cmd/reason string per action)
 * is tested in tests/test_power_menu.c via an injected fake syscall;
 * ui_loop's tap-to-callback dispatch is tested in tests/test_ui_loop.c
 * with mock callbacks. This file is the (intentionally thin, so there's
 * little left to get wrong) glue between those two already-tested
 * pieces, and is not separately re-tested — calling these for real
 * would genuinely reboot/power off whatever machine runs them.
 */

void cf_action_flash_image(const char *image_path, const char *target_partition,
                            const char *device_conf_path);

void cf_action_poweroff(void *user_data);
void cf_action_reboot(void *user_data);
void cf_action_reboot_recovery(void *user_data);
void cf_action_reboot_bootloader(void *user_data);
void cf_action_reboot_download(void *user_data); /* MTK download mode — NOT EDL, see power_menu.h */

#endif /* CF_MENU_ACTIONS_H */
