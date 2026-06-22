#include <gtest/gtest.h>
#include "Fault_Tolerant_I2C_Communication_Layer.h"
#include "Static_Memory+MISRA_Compliance_Layer.h"
#include <new>
#include <string>

static int mock_i2c_err_code=0;
static uint8_t mock_read_data[2]={0,0};

extern "C" {
    int i2c_reg_read_byte(const struct device *dev,uint16_t dev_addr,uint8_t reg_addr,uint8_t *value){
       if (value && mock_i2c_err_code==0) {
          *value=mock_read_data[0];
       }
       return mock_i2c_err_code;
    }

    int i2c_reg_write_byte(const struct device *dev,uint16_t dev_addr,uint8_t reg_addr,uint8_t value){
       return mock_i2c_err_code;
    }
    
    int  i2c_burst_read(const struct device *dev,uint16_t dev_addr,uint8_t start_addr,uint8_t *buff,uint32_t num_bytes){
      if (buff && mock_i2c_err_code==0 && num_bytes>=2){
              buff[0]=mock_read_data[0];
              buff[1]=mock_read_data[1];
      }
      return mock_i2c_err_code;
    }
}

const device* mock_dev = reinterpret_cast<const device*>(0xDEADBEEF);

class I2CFaultRecoveryTestSuite : public ::testing::Test {
  protected:
      void SetUp() override {
          mock_i2c_err_code=0;
          mock_read_data[0]=0;
          mock_read_data[1]=0;
      }
};

TEST_F(I2CFaultRecoveryTestSuite, FailSafeFallback) {
    FailSafeStrategy failsafe;
    
    EXPECT_EQ(failsafe.getLastGood(), 0);

    failsafe.updateLastGood(120);

    failsafe.executeRecovery(mock_dev);
    
    EXPECT_EQ(failsafe.getLastGood(), 120);
}

TEST_F(I2CFaultRecoveryTestSuite, HandlesNACKEncoding) {
    Result<uint8_t> res = Result<uint8_t>::Err(I2CFault::NACK);
    
    EXPECT_FALSE(res.success);
    EXPECT_EQ(res.error, I2CFault::NACK);
    EXPECT_EQ(res.value, 0);
}

TEST_F(I2CFaultRecoveryTestSuite, HandlesTimeoutEncoding) {
      Result<uint16_t> res = Result<uint16_t>::Err(I2CFault::TIMEOUT);
      
      EXPECT_FALSE(res.success);
      EXPECT_EQ(res.error, I2CFault::TIMEOUT);
}

TEST_F(I2CFaultRecoveryTestSuite, HandlesBusBusyEncoding) {
      Result<bool> res = Result<bool>::Err(I2CFault::BUS_BUSY);
      
      EXPECT_FALSE(res.success);
      EXPECT_EQ(res.error, I2CFault::BUS_BUSY);
}

TEST_F(I2CFaultRecoveryTestSuite, HandlesArbitrationLostEncoding) {
      Result<uint8_t> res = Result<uint8_t>::Err(I2CFault::ARBITRATION_LOST);
      
      EXPECT_FALSE(res.success);
      EXPECT_EQ(res.error, I2CFault::ARBITRATION_LOST);
}

TEST_F(I2CFaultRecoveryTestSuite, HandlesSuccessfulRead) {
      uint16_t expected_temp = 3650;
      Result<uint16_t> res = Result<uint16_t>::Ok(expected_temp);
      
      EXPECT_TRUE(res.success);
      EXPECT_EQ(res.error, I2CFault::NONE);
      EXPECT_EQ(res.value, expected_temp);
}

TEST_F(I2CFaultRecoveryTestSuite, StrategySwitchMidStream) {
    RetryStrategy retry;
    BusResetStrategy hard_reset;
    FailSafeStrategy failsafe;
    
    failsafe.updateLastGood(99);
    
    I2CStrategy* active_strategy = nullptr;
    
    active_strategy = &retry;
    EXPECT_NO_FATAL_FAILURE(active_strategy->executeRecovery(mock_dev));
    
    active_strategy = &hard_reset;
    EXPECT_NO_FATAL_FAILURE(active_strategy->executeRecovery(mock_dev));
    
    active_strategy = &failsafe;
    EXPECT_NO_FATAL_FAILURE(active_strategy->executeRecovery(mock_dev));
    
    auto* casted_failsafe = dynamic_cast<FailSafeStrategy*>(active_strategy);
    ASSERT_NE(casted_failsafe, nullptr);
    EXPECT_EQ(casted_failsafe->getLastGood(), 99);
}

TEST_F(I2CFaultRecoveryTestSuite, ManagerReadRegisterSuccess){
    I2CManager manager(mock_dev);
    mock_read_data[0]=42;
    
    auto res=manager.readRegister(0x10,0x20);
    EXPECT_TRUE(res.success);
    EXPECT_EQ(res.value,42);
}

TEST_F(I2CFaultRecoveryTestSuite, ManagerReadRegisterTimeout){
    I2CManager manager(mock_dev);
    mock_i2c_err_code=-ETIMEDOUT;
    
    auto res=manager.readRegister(0x10,0x20);
    EXPECT_FALSE(res.success);
    EXPECT_EQ(res.error,I2CFault::TIMEOUT);
}

TEST_F(I2CFaultRecoveryTestSuite, ManagerReadRegisterNACK){
    I2CManager manager(mock_dev);
    mock_i2c_err_code=-EIO;
    
    auto res=manager.readRegister(0x10,0x20);
    EXPECT_FALSE(res.success);
    EXPECT_EQ(res.error,I2CFault::NACK);
}

TEST_F(I2CFaultRecoveryTestSuite, ManagerWriteRegisterSuccess){
    I2CManager manager(mock_dev);
    
    auto res=manager.writeRegister(0x10,0x20,0xFF);
    EXPECT_TRUE(res.success);
}

TEST_F(I2CFaultRecoveryTestSuite, ManagerWriteRegisterNACK){
    I2CManager manager(mock_dev);
    mock_i2c_err_code=-EIO;
    
    auto res=manager.writeRegister(0x10,0x20,0xFF);
    EXPECT_FALSE(res.success);
    EXPECT_EQ(res.error,I2CFault::NACK);
}

TEST_F(I2CFaultRecoveryTestSuite, ManagerReadWordSuccess){
    I2CManager manager(mock_dev);
    mock_read_data[0]=0xAA;
    mock_read_data[1]=0xBB;
    
    auto res=manager.readWord(0x10,0x20);
    EXPECT_TRUE(res.success);
    EXPECT_EQ(res.value,0xBBAA);
}

TEST_F(I2CFaultRecoveryTestSuite, ManagerReadWordTimeout){
    I2CManager manager(mock_dev);
    mock_i2c_err_code=-ETIMEDOUT;

    auto res=manager.readWord(0x10,0x20);
    EXPECT_FALSE(res.success);
    EXPECT_EQ(res.error,I2CFault::TIMEOUT);
}

TEST_F(I2CFaultRecoveryTestSuite, ManagerReadWordNACK){
    I2CManager manager(mock_dev);
    mock_i2c_err_code=-EIO;

    auto res=manager.readWord(0x10,0x20);
    EXPECT_FALSE(res.success);
    EXPECT_EQ(res.error,I2CFault::NACK);
}

TEST_F(I2CFaultRecoveryTestSuite, VirtualDestructorCoverage){
    StaticPool<RetryStrategy,1> strategy_pool;

    void* raw_memory=strategy_pool.allocate();
    ASSERT_NE(raw_memory,nullptr)<<"Static pool allocation failed!";

    RetryStrategy* strategy=new(raw_memory) RetryStrategy();    

    strategy->~RetryStrategy();

    strategy_pool.deallocate(raw_memory);
}

TEST_F(I2CFaultRecoveryTestSuite,DeallocateNullPointerIsSafe){
    StaticPool<RetryStrategy,1> strategy_pool;
    EXPECT_NO_FATAL_FAILURE(strategy_pool.deallocate(nullptr));
}

TEST_F(I2CFaultRecoveryTestSuite,RetryStrategyLogsWarning){
    RetryStrategy strategy;
    testing::internal::CaptureStdout();

    strategy.executeRecovery(mock_dev);
    
    std::string output=testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("[WRN] I2C Fault Detected. Executing Exponential Backoff Retry...")!= std::string::npos)<<"Expected warning was not printed! Actual output: " << output;
}

TEST_F(I2CFaultRecoveryTestSuite,BusRestStrategyLogsError){
    BusResetStrategy strategy;
    testing::internal::CaptureStdout();

    strategy.executeRecovery(mock_dev);
    
    std::string output=testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("[ERR] I2C Bus Hard-Locked.Toggling SCL to recover")!=std::string::npos)<<"Expected error was not printed! Actual output: "<<output;
}

TEST_F(I2CFaultRecoveryTestSuite,FailSafeStrategyLogsErrorWithValue){
    FailSafeStrategy strategy;
    uint8_t dummy_last_good=188;
    strategy.updateLastGood(dummy_last_good);
  
    testing::internal::CaptureStdout();
    strategy.executeRecovery(mock_dev);
    std::string output =testing::internal::GetCapturedStdout();
    
    EXPECT_TRUE(output.find("Falling back to Last-known-Good data: 188")!= std::string::npos)<<"Expected formatted string was not found! Actual output: "<<output;
}
