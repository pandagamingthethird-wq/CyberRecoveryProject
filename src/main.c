/*
 * main.c — CLI entry point for host-side builds (CI, local testing).
 *
 * On a real device, recovery's init.rc launches this binary as the UI
 * process directly — no argv parsing needed there, since there's no shell.
 * This CLI wrapper exists purely so `make` produces something runnable for
 * CI / local verification, invoking the same action functions the on-device
 * UI would call from menu_actions.c.
 */

#include <stdio.h>
#include <string.h>
#include "ui/ui_theme.h"

static void print_usage(const char *argv0) {
    fprintf(stderr,
        "CyberFlasher (host build)\n"
        "Usage:\n"
        "  %s menu            Show the home menu (verifies UI module links)\n"
        "  %s version          Print version info\n",
        argv0, argv0);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "menu") == 0) {
        cf_ui_draw_menu(CF_MENU_HOME, &CF_THEME_DEFAULT);
        return 0;
    }

    if (strcmp(argv[1], "version") == 0) {
        printf("CyberFlasher host build (CI/test harness)\n");
        return 0;
    }

    print_usage(argv[0]);
    return 1;
}
