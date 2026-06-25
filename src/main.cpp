#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

#include "RTOS_Command_based_thread_system.h"
#include "RTOS_Synchronization_Layer.h"
#include <new>
#include "Device_State_Machine+Watchdog.h"
#include "Smart_Battery_System.h"
#include "Persistent_Configuration_System.h"
#include "Static_Memory+MISRA_Compliance_Layer.h"
#include <zephyr/debug/thread_analyzer.h>
#include "Power_Management_System.h"

LOG_MODULE_REGISTER(MAIN_OS, LOG_LEVEL_INF);

DeviceContext sys_context;

extern ZephyrWorkQueue status_work;

/*
 * QEMU semihost marker for CI only.
 * This does not use Zephyr semihost_poll_out(), so there is no linker issue.
 * It prints through QEMU when the workflow passes -semihosting-config enable=on,target=native.
 */
#if defined(QEMU_RUNTIME_PROBE)
static void qemu_write0(const char *s)
{
    register int op asm("r0") = 0x04;
    register const char *msg asm("r1") = s;

    asm volatile (
        "bkpt 0xab"
        :
        : "r"(op), "r"(msg)
        : "memory"
    );
}
#else
static void qemu_write0(const char * /* s */)
{
}
#endif

int main(void)
{
    qemu_write0("QEMU_BOOT_MARKER_ACTUAL_MAIN\n");
    printk("QEMU_BOOT_MARKER_PRINTK\n");

    LOG_INF("Command-Based RTOS Booting");
    qemu_write0("MAIN_AFTER_LOG_INF\n");

    ConfigStore& config = ConfigStore::getInstance();
    qemu_write0("MAIN_BEFORE_CONFIG_INIT\n");

    if (config.init()) {
        qemu_write0("MAIN_AFTER_CONFIG_INIT\n");

        config.validateEndurance(ConfigKey::ALARM_THRESHOLD);
        qemu_write0("MAIN_AFTER_ENDURANCE_TEST\n");

        uint16_t infusion_rate = 0;

        if (!config.get(ConfigKey::INFUSION_RATE, infusion_rate)) {
            LOG_WRN("First boot detected. Setting default infusion rate.");
            config.set(ConfigKey::INFUSION_RATE, static_cast<uint16_t>(50));
        } else {
            LOG_INF("Loaded Infusion Rate from NVS: %u mL/hr", infusion_rate);
        }
    }

    sys_context.requestTransition(SystemState::RUNNING);
    qemu_write0("MAIN_AFTER_STATE_RUNNING\n");

    status_work.schedule(K_SECONDS(1));
    qemu_write0("MAIN_AFTER_STATUS_WORK_SCHEDULE\n");

    return 0;
}
