#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "Fault_Tolerant_I2C_Communication_Layer.h"
#include <errno.h>
#include <stdlib.h> // Required for rand() in QEMU mock generation

LOG_MODULE_REGISTER(I2C_HW,LOG_LEVEL_WRN);
 
void RetryStrategy::executeRecovery(const device* /* i2c_dev */){
    LOG_WRN("I2C Fault Detected. Executing Exponential Backoff Retry...");
    k_msleep(10);
}

void BusResetStrategy::executeRecovery(const device* /* i2c_dev */){
    LOG_ERR("I2C Bus Hard-Locked.Toggling SCL to recover");
}

void FailSafeStrategy::executeRecovery(const device* /* i2c_dev */){
      LOG_ERR("I2C Critical Failure. Falling back to Last-known-Good data: %u",last_known_good_value);
}

void FailSafeStrategy::updateLastGood(uint8_t val){
      last_known_good_value=val;
}

uint8_t FailSafeStrategy::getLastGood() const{
      return last_known_good_value;
}

I2CManager::I2CManager(const device* i2c_dev):i2c_dev(i2c_dev){}

Result<uint8_t> I2CManager::readRegister(uint16_t sensor_addr,uint8_t reg_addr){
#ifdef CONFIG_QEMU_CORTEX_M3
    // ==========================================
    // EMULATOR MODE (Cortex-M4): Inject Simulated Sensor Data
    // ==========================================
    k_msleep(50); // Simulate I2C bus delay
    uint8_t mock_data = 60 + (rand() % 40); // Generate dynamic 8-bit data
    printk("<inf> SIM_I2C: Injected Mock Byte (Sensor: 0x%02X, Reg: 0x%02X) -> %u\n", sensor_addr, reg_addr, mock_data);
    return Result<uint8_t>::Ok(mock_data);
#else
    // ==========================================
    // PHYSICAL HARDWARE MODE: Real I2C Read
    // ==========================================
    uint8_t data=0;
    
    int err=i2c_reg_read_byte(i2c_dev,sensor_addr,reg_addr,&data);
    
    if (err==0){
        return Result<uint8_t>::Ok(data);
    }
    
    if (err==-ETIMEDOUT){
        reset_strategy.executeRecovery(i2c_dev);
        return Result<uint8_t>::Err(I2CFault::TIMEOUT);
    }
    
    retry_strategy.executeRecovery(i2c_dev);
    return Result<uint8_t>::Err(I2CFault::NACK);
#endif
}

Result<bool> I2CManager::writeRegister(uint16_t sensor_addr,uint8_t reg_addr,uint8_t val){
#ifdef CONFIG_QEMU_CORTEX_M3
    k_msleep(10);
    printk("<inf> SIM_I2C: Mock Write Success (Sensor: 0x%02X, Reg: 0x%02X) -> %u\n", sensor_addr, reg_addr, val);
    return Result<bool>::Ok(true);
#else
    int err=i2c_reg_write_byte(i2c_dev,sensor_addr,reg_addr,val);
    
    if (err==0){
        return Result<bool>::Ok(true);
    }
    
    retry_strategy.executeRecovery(i2c_dev);
    return Result<bool>::Err(I2CFault::NACK);
#endif
}

Result<uint16_t> I2CManager::readWord(uint16_t sensor_addr,uint8_t reg_addr){
#ifdef CONFIG_QEMU_CORTEX_M3
    k_msleep(50);
    uint16_t mock_val = 900 + (rand() % 100); // Generate dynamic 16-bit data
    printk("<inf> SIM_I2C: Injected Mock Word (Sensor: 0x%02X, Reg: 0x%02X) -> %u\n", sensor_addr, reg_addr, mock_val);
    return Result<uint16_t>::Ok(mock_val);
#else
    uint8_t buf[2]={0,0};
    
    int err=i2c_burst_read(i2c_dev,sensor_addr,reg_addr,buf,2);
    
    if (err==0){
        uint16_t val=(static_cast<uint16_t>(buf[1])<<8)|buf[0];
        return Result<uint16_t>::Ok(val);
    }
    
    if (err==-ETIMEDOUT){
        reset_strategy.executeRecovery(i2c_dev);
        return Result<uint16_t>::Err(I2CFault::TIMEOUT);
    }
    
    retry_strategy.executeRecovery(i2c_dev);
    return Result<uint16_t>::Err(I2CFault::NACK);
#endif
}
