#include "Device_State_Machine+Watchdog.h"
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>

LOG_MODULE_REGISTER(DEVICE_STATE, LOG_LEVEL_WRN);
  
WatchdogTimer::WatchdogTimer():wdt_dev(DEVICE_DT_GET(DT_NODELABEL(iwdg))),channel_id(-1){}

bool WatchdogTimer::init(uint32_t timeout_ms){
    if (!device_is_ready(wdt_dev)){
      return false;
    }
    
    struct wdt_timeout_cfg wdt_config={
        .window={ .min=0, .max=timeout_ms},
        .callback=nullptr,
        .flags=WDT_FLAG_RESET_SOC
    };
    
    channel_id=wdt_install_timeout(wdt_dev,&wdt_config);
    
    if (channel_id<0){
      return false;
    }
    
    wdt_setup(wdt_dev, WDT_OPT_PAUSE_HALTED_BY_DBG);
    return true;
}

void WatchdogTimer::feed(){
    if (channel_id>=0){
      wdt_feed(wdt_dev,channel_id);
    }
}

DeviceContext::DeviceContext():current_state(SystemState::INIT){}

SystemState DeviceContext::getState() const{
  return current_state;
}

bool DeviceContext::requestTransition(SystemState next_state){
    if (current_state==SystemState::FAULT|| current_state ==SystemState::SAFE_HALT){
        LOG_ERR("Cannot transition out of FAULT/HALT state normally.");
        return false;
    }
    
    current_state=next_state;
    LOG_INF("System transitioned to state:%d",static_cast<int>(current_state));
    return true;
}

void DeviceContext::triggerFault(const char* reason){
    LOG_ERR("CRITICAL FAULT: %s. Forcing SAFE_HALT.",reason);
    current_state=SystemState::SAFE_HALT;
}

