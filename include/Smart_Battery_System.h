#include "Fault_Tolerant_I2C_Communication_Layer.h"
#include "Device_State_Machine+Watchdog.h"
#include <cstdint>
#pragma once

enum class BatteryFSM{IDLE,CHARGING,DISCHARGING,CUTOFF};

struct BatteryLimits{
    static constexpr uint8_t CUTOFF_SOC_PCT=10;
    static constexpr uint8_t REENABLE_SOC_PCT=15;
};

class SbsBattery{
  private:
      I2CManager* i2c_bus;
      DeviceContext* sys_context;
      BatteryFSM current_state;
      
      static constexpr uint16_t SBS_ADDR=0x0B;
      static constexpr uint8_t REG_TEMP=0x08;
      static constexpr uint8_t REG_VOLTAGE=0x09;
      static constexpr uint8_t REG_CURRENT=0x0A;
      static constexpr uint8_t REG_SOC=0x0D;
      static constexpr uint8_t REG_CAPACITY=0x0F;
      
      bool full_charge_logged;
      
  public:
      SbsBattery(I2CManager* bus, DeviceContext* context);
      
      Result<uint16_t> getVoltage();
      Result<int16_t> getCurrent();
      Result<uint8_t> getStateOfCharge();
      Result<uint16_t> getTemperature();
      Result<uint16_t> getCapacity();
      
      void processFSM();
      BatteryFSM getState() const;
};

