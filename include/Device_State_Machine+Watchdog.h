#include <zephyr/kernel.h>
#include <zephyr/drivers/watchdog.h>
#pragma once

enum class SystemState{
  INIT,
  RUNNING,
  FAULT,
  SAFE_HALT
};

class WatchdogTimer{
    private:
        const device* wdt_dev;
        int channel_id;
    public:
        WatchdogTimer();
        bool init(uint32_t timeout_ms);
        void feed();
};

class DeviceContext {
    private:
        SystemState current_state;
        WatchdogTimer wdt;
    public:
        DeviceContext();
        
        SystemState getState() const;
        bool requestTransition(SystemState next_state);
        
        void triggerFault(const char* reason);
};
        
