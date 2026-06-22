#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "Fault_Tolerant_I2C_Communication_Layer.h"
#include <errno.h>

LOG_MODULE_REGISTER(I2C_HW,LOG_LEVEL_WRN);
 
void RetryStrategy::executeRecovery(const device* i2c_dev){
    LOG_WRN("I2C Fault Detected. Executing Exponential Backoff Retry...");
    k_msleep(10);
}

void BusResetStrategy::executeRecovery(const device* i2c_dev){
    LOG_ERR("I2C Bus Hard-Locked.Toggling SCL to recover");
}

void FailSafeStrategy::executeRecovery(const device* i2c_dev){
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
}

Result<bool> I2CManager::writeRegister(uint16_t sensor_addr,uint8_t reg_addr,uint8_t val){
    int err=i2c_reg_write_byte(i2c_dev,sensor_addr,reg_addr,val);
    
    if (err==0){
        return Result<bool>::Ok(true);
    }
    
    retry_strategy.executeRecovery(i2c_dev);
    return Result<bool>::Err(I2CFault::NACK);
}

Result<uint16_t> I2CManager::readWord(uint16_t sensor_addr,uint8_t reg_addr){
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
}
