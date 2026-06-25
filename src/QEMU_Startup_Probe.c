#include <stdint.h>
#include <zephyr/init.h>

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

static int qemu_probe_pre_kernel_1_p0(void)
{
    qemu_write0("QEMU_PROBE_PRE_KERNEL_1_P0\n");
    return 0;
}

static int qemu_probe_pre_kernel_1_p10(void)
{
    qemu_write0("QEMU_PROBE_PRE_KERNEL_1_P10\n");
    return 0;
}

static int qemu_probe_pre_kernel_1_p20(void)
{
    qemu_write0("QEMU_PROBE_PRE_KERNEL_1_P20\n");
    return 0;
}

static int qemu_probe_pre_kernel_1_p30(void)
{
    qemu_write0("QEMU_PROBE_PRE_KERNEL_1_P30\n");
    return 0;
}

static int qemu_probe_pre_kernel_1_p40(void)
{
    qemu_write0("QEMU_PROBE_PRE_KERNEL_1_P40\n");
    return 0;
}

static int qemu_probe_pre_kernel_1_p50(void)
{
    qemu_write0("QEMU_PROBE_PRE_KERNEL_1_P50\n");
    return 0;
}

static int qemu_probe_pre_kernel_1_p60(void)
{
    qemu_write0("QEMU_PROBE_PRE_KERNEL_1_P60\n");
    return 0;
}

static int qemu_probe_pre_kernel_1_p70(void)
{
    qemu_write0("QEMU_PROBE_PRE_KERNEL_1_P70\n");
    return 0;
}

static int qemu_probe_pre_kernel_1_p80(void)
{
    qemu_write0("QEMU_PROBE_PRE_KERNEL_1_P80\n");
    return 0;
}

static int qemu_probe_pre_kernel_1_p90(void)
{
    qemu_write0("QEMU_PROBE_PRE_KERNEL_1_P90\n");
    return 0;
}

static int qemu_probe_pre_kernel_2_p0(void)
{
    qemu_write0("QEMU_PROBE_PRE_KERNEL_2_P0\n");
    return 0;
}

static int qemu_probe_post_kernel_p0(void)
{
    qemu_write0("QEMU_PROBE_POST_KERNEL_P0\n");
    return 0;
}

static int qemu_probe_application_p0(void)
{
    qemu_write0("QEMU_PROBE_APPLICATION_P0\n");
    return 0;
}

SYS_INIT(qemu_probe_pre_kernel_1_p0, PRE_KERNEL_1, 0);
SYS_INIT(qemu_probe_pre_kernel_1_p10, PRE_KERNEL_1, 10);
SYS_INIT(qemu_probe_pre_kernel_1_p20, PRE_KERNEL_1, 20);
SYS_INIT(qemu_probe_pre_kernel_1_p30, PRE_KERNEL_1, 30);
SYS_INIT(qemu_probe_pre_kernel_1_p40, PRE_KERNEL_1, 40);
SYS_INIT(qemu_probe_pre_kernel_1_p50, PRE_KERNEL_1, 50);
SYS_INIT(qemu_probe_pre_kernel_1_p60, PRE_KERNEL_1, 60);
SYS_INIT(qemu_probe_pre_kernel_1_p70, PRE_KERNEL_1, 70);
SYS_INIT(qemu_probe_pre_kernel_1_p80, PRE_KERNEL_1, 80);
SYS_INIT(qemu_probe_pre_kernel_1_p90, PRE_KERNEL_1, 90);

SYS_INIT(qemu_probe_pre_kernel_2_p0, PRE_KERNEL_2, 0);
SYS_INIT(qemu_probe_post_kernel_p0, POST_KERNEL, 0);
SYS_INIT(qemu_probe_application_p0, APPLICATION, 0);
