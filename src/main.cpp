#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#ifdef QEMU_SMOKE_TEST

int main(void) {
    int counter = 0;

    while (true) {
        printk("QEMU_BOOT_MARKER %d\n", counter++);
        k_msleep(1000);
    }

    return 0;
}

#else

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

int main(void) {
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
