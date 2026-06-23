#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include "Smart_Battery_System.h"
#include <zephyr/device.h>
#ifdef IS_TEST_ENVIRONMENT
    extern bool run_thread_once;
    #define THREAD_LOOP_CONDITION (run_thread_once ? (run_thread_once = false, true) : false)
#else
    #define THREAD_LOOP_CONDITION true
#endif
extern DeviceContext sys_context;
LOG_MODULE_REGISTER(BATTERY_SYS,LOG_LEVEL_INF);

const device* i2c_hardware= DEVICE_DT_GET(DT_NODELABEL(i2c1));
I2CManager i2c_bus_manager(i2c_hardware);
SbsBattery smart_battery(&i2c_bus_manager,&sys_context);

SbsBattery::SbsBattery(I2CManager* bus, DeviceContext* context): i2c_bus(bus), sys_context(context), current_state(BatteryFSM::IDLE), full_charge_logged(false){}

Result<uint16_t> SbsBattery::getVoltage(){
    return i2c_bus->readWord(SBS_ADDR, REG_VOLTAGE);
}

Result<int16_t> SbsBattery::getCurrent(){
    auto res=i2c_bus->readWord(SBS_ADDR, REG_CURRENT);
    if (!res.success) {
        return Result<int16_t>::Err(res.error);
    }
    return Result<int16_t>::Ok(static_cast<int16_t>(res.value));
}

Result<uint8_t> SbsBattery::getStateOfCharge(){
    auto res=i2c_bus->readWord(SBS_ADDR, REG_SOC);
    if (!res.success){
        return Result<uint8_t>::Err(res.error);
    }
    return Result<uint8_t>::Ok(static_cast<uint8_t>(res.value));
}

Result<uint16_t> SbsBattery::getTemperature(){
    return i2c_bus->readWord(SBS_ADDR, REG_TEMP);
}

Result<uint16_t>  SbsBattery::getCapacity(){
    return i2c_bus->readWord(SBS_ADDR, REG_CAPACITY);
}
void SbsBattery::processFSM(){
    auto current_res=getCurrent();
    auto soc_res=getStateOfCharge();
    
    if (!current_res.success||!soc_res.success){
        LOG_ERR("Battery communication failure. Maintaining current state.");
        return;
    }
    
    int16_t current_ma=current_res.value;
    uint8_t soc_pct=soc_res.value;
    
    if (current_state != BatteryFSM::CUTOFF) {
        if (current_ma>0){
            current_state=BatteryFSM::CHARGING;
        } 
        else if(current_ma<0){
            current_state=BatteryFSM::DISCHARGING;
        }  
        else {
            current_state=BatteryFSM::IDLE;
        }
    }
    
    if (soc_pct>=100 && !full_charge_logged){
        LOG_INF("BATTERY FULLY CHARGED.Triggering NVS Log entry.");
        full_charge_logged=true;
    }
    else if(soc_pct<95){
        full_charge_logged=false;
    }
    
    if (soc_pct <= BatteryLimits::CUTOFF_SOC_PCT){
      if (current_state != BatteryFSM::CUTOFF){
          LOG_ERR("DISCHARGE GUARD TRIGGERED! SoC:%u%%. Halting System.",soc_pct);
          sys_context->triggerFault("Battery Critically Low");
          current_state=BatteryFSM::CUTOFF;
      }
    }
    else if(soc_pct>=BatteryLimits::REENABLE_SOC_PCT && sys_context->getState()==SystemState::SAFE_HALT){
        LOG_INF("Battery recovered to %u%%.System safe to restart.", soc_pct);
        sys_context->requestTransition(SystemState::INIT);
        
        current_state=(current_ma>0)? BatteryFSM::CHARGING : BatteryFSM::IDLE;
    }
}

 BatteryFSM SbsBattery::getState() const{
    return current_state;
 }  
    
void battery_monitor_thread(void){
    if (!device_is_ready(i2c_hardware)){
        LOG_ERR("I2C hardware not ready. Battery monitor halting.");
        return;
    }
    
    do{smart_battery.processFSM();
        
        if (smart_battery.getState()==BatteryFSM::DISCHARGING){
            auto soc=smart_battery.getStateOfCharge();
            if (soc.success){
                LOG_INF("Battery Discharging: %u%% remaining",soc.value);
            }
        }
        
        k_msleep(1000);
    }while(THREAD_LOOP_CONDITION);
}

K_THREAD_DEFINE(battery_tid,512,battery_monitor_thread,NULL,NULL,NULL,10,0,0);
