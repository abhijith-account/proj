#include <stdint.h>
#include <zephyr/init.h>

/*
 * Direct ARM semihosting write0.
 * This avoids Zephyr UART/logging/printk and does not depend on semihost_poll_out().
 *
 * Works only when QEMU is launched with:
 * -semihosting-config enable=on,target=native
 */
static void qemu_write0(const char *s)
{
    register uint32_t op __asm__("r0") = 0x04;
    register const char *msg __asm__("r1") = s;

    __asm__ volatile (
        "bkpt 0xab"
        :
        : "r"(op), "r"(msg)
        : "memory"
    );
}

static int qemu_probe_pre_kernel_1(void)
{
    qemu_write0("QEMU_PROBE_PRE_KERNEL_1\n");
    return 0;
}

static int qemu_probe_pre_kernel_2(void)
{
    qemu_write0("QEMU_PROBE_PRE_KERNEL_2\n");
    return 0;
}

static int qemu_probe_post_kernel(void)
{
    qemu_write0("QEMU_PROBE_POST_KERNEL\n");
    return 0;
}

static int qemu_probe_application(void)
{
    qemu_write0("QEMU_PROBE_APPLICATION_INIT\n");
    return 0;
}

SYS_INIT(qemu_probe_pre_kernel_1, PRE_KERNEL_1, 0);
SYS_INIT(qemu_probe_pre_kernel_2, PRE_KERNEL_2, 0);
SYS_INIT(qemu_probe_post_kernel, POST_KERNEL, 0);
SYS_INIT(qemu_probe_application, APPLICATION, 0);
