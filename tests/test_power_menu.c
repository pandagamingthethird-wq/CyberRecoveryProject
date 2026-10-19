/* test_power_menu.c — tests cf_power_execute()'s dispatch logic (right
 * magic numbers, right cmd, right reason string per action) via an
 * injected fake syscall that just RECORDS the call instead of actually
 * rebooting the test machine. cf_power_real_syscall() itself is never
 * invoked here — doing so would genuinely reboot/power off whatever
 * machine runs this test. */
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "power_menu.h"

typedef struct {
    int magic1, magic2, cmd;
    char arg[32];
    int call_count;
} captured_call_t;

static captured_call_t g_captured;

static int fake_syscall(int magic1, int magic2, int cmd, const char *arg) {
    g_captured.magic1 = magic1;
    g_captured.magic2 = magic2;
    g_captured.cmd = cmd;
    snprintf(g_captured.arg, sizeof(g_captured.arg), "%s", arg ? arg : "");
    g_captured.call_count++;
    return 0; /* pretend success */
}

#define LINUX_REBOOT_MAGIC1      0xfee1dead
#define LINUX_REBOOT_MAGIC2      0x28121969
#define LINUX_REBOOT_CMD_RESTART    0x01234567
#define LINUX_REBOOT_CMD_POWER_OFF  0x4321FEDC
#define LINUX_REBOOT_CMD_RESTART2   0xA1B2C3D4

static void reset_capture(void) { memset(&g_captured, 0, sizeof(g_captured)); }

static int fake_syscall_fail(int magic1, int magic2, int cmd, const char *arg) {
    (void)magic1; (void)magic2; (void)cmd; (void)arg;
    return -1; /* pretend the kernel rejected it */
}

int main() {
    cf_power_options_t opts = { .syscall_fn = fake_syscall };

    reset_capture();
    assert(cf_power_execute(CF_POWER_OFF, &opts) == true);
    assert(g_captured.call_count == 1);
    assert((unsigned)g_captured.magic1 == LINUX_REBOOT_MAGIC1);
    assert((unsigned)g_captured.magic2 == LINUX_REBOOT_MAGIC2);
    assert((unsigned)g_captured.cmd == LINUX_REBOOT_CMD_POWER_OFF);
    printf("CF_POWER_OFF -> magic1=0x%x magic2=0x%x cmd=0x%x arg='%s'\n",
           g_captured.magic1, g_captured.magic2, g_captured.cmd, g_captured.arg);

    reset_capture();
    assert(cf_power_execute(CF_POWER_REBOOT, &opts) == true);
    assert((unsigned)g_captured.cmd == LINUX_REBOOT_CMD_RESTART);
    assert(strlen(g_captured.arg) == 0); /* plain restart has no reason string */
    printf("CF_POWER_REBOOT -> cmd=0x%x arg='%s'\n", g_captured.cmd, g_captured.arg);

    reset_capture();
    assert(cf_power_execute(CF_POWER_REBOOT_RECOVERY, &opts) == true);
    assert((unsigned)g_captured.cmd == LINUX_REBOOT_CMD_RESTART2);
    assert(strcmp(g_captured.arg, "recovery") == 0);
    printf("CF_POWER_REBOOT_RECOVERY -> cmd=0x%x arg='%s'\n", g_captured.cmd, g_captured.arg);

    reset_capture();
    assert(cf_power_execute(CF_POWER_REBOOT_BOOTLOADER, &opts) == true);
    assert((unsigned)g_captured.cmd == LINUX_REBOOT_CMD_RESTART2);
    assert(strcmp(g_captured.arg, "bootloader") == 0);
    printf("CF_POWER_REBOOT_BOOTLOADER -> cmd=0x%x arg='%s'\n", g_captured.cmd, g_captured.arg);

    reset_capture();
    assert(cf_power_execute(CF_POWER_REBOOT_DOWNLOAD, &opts) == true);
    assert((unsigned)g_captured.cmd == LINUX_REBOOT_CMD_RESTART2);
    assert(strcmp(g_captured.arg, "download") == 0);
    printf("CF_POWER_REBOOT_DOWNLOAD -> cmd=0x%x arg='%s'\n", g_captured.cmd, g_captured.arg);

    /* labels exist and are distinct */
    assert(strcmp(cf_power_action_label(CF_POWER_OFF), "Power Off") == 0);
    assert(strcmp(cf_power_action_label(CF_POWER_REBOOT_RECOVERY), "Reboot to Recovery") == 0);
    assert(strcmp(cf_power_action_label(CF_POWER_REBOOT_BOOTLOADER), "Reboot to Fastboot") == 0);
    assert(strcmp(cf_power_action_label(CF_POWER_REBOOT_DOWNLOAD), "Reboot to Download Mode") == 0);

    /* a syscall "failure" (nonzero return) should propagate as false */
    cf_power_options_t fail_opts = { .syscall_fn = fake_syscall_fail };
    assert(cf_power_execute(CF_POWER_OFF, &fail_opts) == false);
    printf("Syscall failure correctly propagates as cf_power_execute() returning false\n");

    printf("\nALL POWER_MENU TESTS PASSED (dispatch logic only — "
           "real syscall never invoked)\n");
    return 0;
}
