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

#define MAKE_PROBE(name, text)        \
    static int name(void)             \
    {                                 \
        qemu_write0(text "\n");       \
        return 0;                     \
    }

MAKE_PROBE(qemu_probe_pre_kernel_1_p0,  "QEMU_PROBE_PRE_KERNEL_1_P0")
MAKE_PROBE(qemu_probe_pre_kernel_1_p1,  "QEMU_PROBE_PRE_KERNEL_1_P1")
MAKE_PROBE(qemu_probe_pre_kernel_1_p2,  "QEMU_PROBE_PRE_KERNEL_1_P2")
MAKE_PROBE(qemu_probe_pre_kernel_1_p3,  "QEMU_PROBE_PRE_KERNEL_1_P3")
MAKE_PROBE(qemu_probe_pre_kernel_1_p4,  "QEMU_PROBE_PRE_KERNEL_1_P4")
MAKE_PROBE(qemu_probe_pre_kernel_1_p5,  "QEMU_PROBE_PRE_KERNEL_1_P5")
MAKE_PROBE(qemu_probe_pre_kernel_1_p6,  "QEMU_PROBE_PRE_KERNEL_1_P6")
MAKE_PROBE(qemu_probe_pre_kernel_1_p7,  "QEMU_PROBE_PRE_KERNEL_1_P7")
MAKE_PROBE(qemu_probe_pre_kernel_1_p8,  "QEMU_PROBE_PRE_KERNEL_1_P8")
MAKE_PROBE(qemu_probe_pre_kernel_1_p9,  "QEMU_PROBE_PRE_KERNEL_1_P9")
MAKE_PROBE(qemu_probe_pre_kernel_1_p10, "QEMU_PROBE_PRE_KERNEL_1_P10")

MAKE_PROBE(qemu_probe_pre_kernel_2_p0,  "QEMU_PROBE_PRE_KERNEL_2_P0")
MAKE_PROBE(qemu_probe_post_kernel_p0,   "QEMU_PROBE_POST_KERNEL_P0")
MAKE_PROBE(qemu_probe_application_p0,   "QEMU_PROBE_APPLICATION_P0")

SYS_INIT(qemu_probe_pre_kernel_1_p0,  PRE_KERNEL_1, 0);
SYS_INIT(qemu_probe_pre_kernel_1_p1,  PRE_KERNEL_1, 1);
SYS_INIT(qemu_probe_pre_kernel_1_p2,  PRE_KERNEL_1, 2);
SYS_INIT(qemu_probe_pre_kernel_1_p3,  PRE_KERNEL_1, 3);
SYS_INIT(qemu_probe_pre_kernel_1_p4,  PRE_KERNEL_1, 4);
SYS_INIT(qemu_probe_pre_kernel_1_p5,  PRE_KERNEL_1, 5);
SYS_INIT(qemu_probe_pre_kernel_1_p6,  PRE_KERNEL_1, 6);
SYS_INIT(qemu_probe_pre_kernel_1_p7,  PRE_KERNEL_1, 7);
SYS_INIT(qemu_probe_pre_kernel_1_p8,  PRE_KERNEL_1, 8);
SYS_INIT(qemu_probe_pre_kernel_1_p9,  PRE_KERNEL_1, 9);
SYS_INIT(qemu_probe_pre_kernel_1_p10, PRE_KERNEL_1, 10);

SYS_INIT(qemu_probe_pre_kernel_2_p0, PRE_KERNEL_2, 0);
SYS_INIT(qemu_probe_post_kernel_p0, POST_KERNEL, 0);
SYS_INIT(qemu_probe_application_p0, APPLICATION, 0);
