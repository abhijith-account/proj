#include <zephyr/kernel.h>

#if defined(CONFIG_BOARD_OLIMEX_STM32_H405)

extern "C" {
#include <zephyr/arch/common/semihost.h>
}

static void qemu_semihost_puts(const char *s)
{
    while (*s != '\0') {
        semihost_poll_out(*s++);
    }
}

int main(void)
{
    int counter = 0;

    while (true) {
        qemu_semihost_puts("QEMU_BOOT_MARKER ");
        semihost_poll_out(static_cast<char>('0' + (counter % 10)));
        semihost_poll_out('\n');

        counter++;
        k_msleep(1000);
    }

    return 0;
}

#else

#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
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

int main(void)
{
    printk("QEMU_BOOT_MARKER\n");
    LOG_INF("Command-Based RTOS Booting");

    ConfigStore& config = ConfigStore::getInstance();
    if (config.init()) {
        config.validateEndurance(ConfigKey::ALARM_THRESHOLD);

        uint16_t infusion_rate = 0;
        if (!config.get(ConfigKey::INFUSION_RATE, infusion_rate)) {
            LOG_WRN("First boot detected. Setting default infusion rate.");
            config.set(ConfigKey::INFUSION_RATE, static_cast<uint16_t>(50));
        } else {
            LOG_INF("Loaded Infusion Rate from NVS: %u mL/hr", infusion_rate);
        }
    }

    sys_context.requestTransition(SystemState::RUNNING);
    status_work.schedule(K_SECONDS(1));

    return 0;
}

#endif
