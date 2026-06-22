#include <zephyr/drivers/i2c.h>
#include <cstdint>
#pragma once

enum class I2CFault{NONE,NACK,TIMEOUT,BUS_BUSY,ARBITRATION_LOST};

template <typename T>
struct Result{
    T value;
    I2CFault error;
    bool success;
    
    static Result<T> Ok(T val){
        return {val,I2CFault::NONE,true};
    }
    
    static Result<T> Err(I2CFault err){
        return {T(),err,false};
    }
};

class I2CStrategy{
  public:
      protected:
        ~I2CStrategy()=default;
      public:
          virtual void executeRecovery(const device* i2c_dev)=0;
};

class RetryStrategy:public I2CStrategy{
  public:
      void executeRecovery(const device* i2c_dev) override;
};

class BusResetStrategy:public I2CStrategy{
  public:
      void executeRecovery(const device* i2c_dev) override;
};

class FailSafeStrategy:public I2CStrategy{
  private:
      uint8_t last_known_good_value=0;
  public:
      void executeRecovery(const device* i2c_dev) override;
      void updateLastGood(uint8_t val);
      uint8_t getLastGood() const;
};

class I2CManager{
  private:
      const device* i2c_dev;
      RetryStrategy retry_strategy;
      BusResetStrategy reset_strategy;  
  public:
      explicit I2CManager(const device* dev);
      Result<uint8_t> readRegister(uint16_t sensor_addr,uint8_t reg_addr);
      Result<bool> writeRegister(uint16_t sensor_addr,uint8_t reg_addr,uint8_t val);
      Result<uint16_t> readWord(uint16_t sensor_addr,uint8_t reg_addr);
};



