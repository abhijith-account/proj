#include <gtest/gtest.h>
#include <zephyr/kernel.h>

#include "Smart_Battery_System.h"
#include "Device_State_Machine+Watchdog.h"
extern SbsBattery smart_battery;
DeviceContext sys_context;
int16_t mock_battery_current_ma=0;
uint16_t mock_battery_soc_pct=50;
bool mock_i2c_fail=false;
bool mock_soc_fail=false;
bool mock_device_ready=true;

extern "C" {
    bool device_is_ready(const struct device *dev){
          return mock_device_ready;
    }
    int i2c_reg_read_byte(const struct device *dev,uint16_t dev_addr,uint8_t reg_addr,uint8_t *value){
          return 0;
    }
    int i2c_reg_write_byte(const struct device *dev,uint16_t dev_addr,uint8_t reg_addr,uint8_t value){
          return 0;
    }
    int wdt_install_timeout(const struct device *dev,const struct wdt_timeout_cfg *cfg){
          return 0;
    }
    int wdt_setup(const struct device *dev,uint8_t options){
          return 0;
    }
    int wdt_feed(const struct device *dev,int channel_id){
          return 0;
    }
    int i2c_burst_read(const struct device *dev,uint16_t dev_addr,uint8_t start_addr,uint8_t *buf,uint32_t num_bytes){
    if (mock_i2c_fail){
      return -1;
    }
    if (mock_soc_fail && start_addr==0x0D){
      return -1;
    }
    if (start_addr==0x0A){
      buf[0]=mock_battery_current_ma &0xFF;
      buf[1]=(mock_battery_current_ma>>8)&0xFF;
    }
    else {
      buf[0]=mock_battery_soc_pct & 0xFF;
      buf[1]=0;
    }
    return 0;
  }
}

class BatteryFSMTestSuite:public::testing::Test{
    protected:
        const device* dummy_i2c=reinterpret_cast<const device*>(0xBEEF);
        
        DeviceContext sys_context;
        I2CManager i2c_manager{dummy_i2c};
        SbsBattery battery{&i2c_manager,&sys_context};
        
        void SetUp() override{
            sys_context.requestTransition(SystemState::RUNNING);
            mock_battery_current_ma=0;
            mock_battery_soc_pct=100;
            mock_i2c_fail=false;
            mock_soc_fail=false;
            mock_device_ready=true;
        }
};

TEST_F(BatteryFSMTestSuite, CurrentTheresholdTransition){
    mock_battery_current_ma=0;
    battery.processFSM();
    EXPECT_EQ(battery.getState(),BatteryFSM::IDLE);
    
    mock_battery_current_ma=500;
    battery.processFSM();
    EXPECT_EQ(battery.getState(),BatteryFSM::CHARGING);
    
    mock_battery_current_ma=-250;
    battery.processFSM();
    EXPECT_EQ(battery.getState(),BatteryFSM::DISCHARGING);
}

TEST_F(BatteryFSMTestSuite,DischargeGuardHysteresisLogic){
      mock_battery_current_ma=-100;
      mock_battery_soc_pct=20;
      battery.processFSM();

      EXPECT_EQ(battery.getState(),BatteryFSM::DISCHARGING);
      EXPECT_EQ(sys_context.getState(),SystemState::RUNNING);
      
      mock_battery_soc_pct=10;
      testing::internal::CaptureStdout();
      battery.processFSM();
      std::string output_cutoff=testing::internal::GetCapturedStdout();
      
      EXPECT_EQ(battery.getState(),BatteryFSM::CUTOFF);
      EXPECT_EQ(sys_context.getState(),SystemState::SAFE_HALT);
      EXPECT_TRUE(output_cutoff.find("[ERR] DISCHARGE GUARD TRIGGERED! SoC:10%. Halting System.")!=std::string::npos)<<"Cutoff error log missing! Actual: "<<output_cutoff;
      
      battery.processFSM();
      
      mock_battery_soc_pct=12;
      battery.processFSM();
      
      mock_battery_soc_pct=15;
      testing::internal::CaptureStdout();
      battery.processFSM();
      std::string output_recovery=testing::internal::GetCapturedStdout(); 
      
      EXPECT_EQ(sys_context.getState(), SystemState::SAFE_HALT)<<"CRITICAL SAFETY FAILURE: Peripheral overrode the SAFE_HALT lock!";
      EXPECT_TRUE(output_recovery.find("[INF] Battery recovered to 15%.System safe to restart.") != std::string::npos)<<"Recovery info log missing! Actual: "<< output_recovery;
}

TEST_F(BatteryFSMTestSuite,FullChargeNVSLogging){
      mock_battery_soc_pct=100;
      testing::internal::CaptureStdout();
      EXPECT_NO_FATAL_FAILURE(battery.processFSM());
      std::string output_full=testing::internal::GetCapturedStdout();
      EXPECT_TRUE(output_full.find("[INF] BATTERY FULLY CHARGED.Triggering NVS Log entry")!= std::string::npos);
      
      battery.processFSM();
      
      mock_battery_soc_pct=98;
      battery.processFSM();

      mock_battery_soc_pct=94;
      EXPECT_NO_FATAL_FAILURE(battery.processFSM());
      
      mock_battery_soc_pct=100;
      testing::internal::CaptureStdout();
      EXPECT_NO_FATAL_FAILURE(battery.processFSM());
      std::string output_recharge=testing::internal::GetCapturedStdout();
      EXPECT_TRUE(output_recharge.find("[INF] BATTERY FULLY CHARGED.Triggering NVS Log entry")!= std::string::npos);
}

TEST_F(BatteryFSMTestSuite,HardwareFailureMaintainState){
    mock_battery_current_ma=-100;
    battery.processFSM();
    ASSERT_EQ(battery.getState(),BatteryFSM::DISCHARGING);

    mock_i2c_fail=true;
    testing::internal::CaptureStdout();
    battery.processFSM();
    std::string output_fail=testing::internal::GetCapturedStdout();

    EXPECT_EQ(battery.getState(),BatteryFSM::DISCHARGING)<<"FSM transitioned unexpectedly during an I2C hardware fault!";
    EXPECT_TRUE(output_fail.find("[ERR] Battery communication failure. Maintaining current state.")!= std::string::npos)<<"Hardware failure log missing! Actual: "<< output_fail;
}

TEST_F(BatteryFSMTestSuite, PartialHardwareFailureSoCOnly){
    mock_battery_current_ma=100;
    mock_soc_fail=true;
    
    testing::internal::CaptureStdout();
    battery.processFSM();
    std::string output= testing::internal::GetCapturedStdout();
    
    EXPECT_TRUE(output.find("[ERR] Battery communication failure.") != std::string::npos);
}

TEST_F(BatteryFSMTestSuite,ReadsAncillaryDataGetters){
    auto voltage=battery.getVoltage();
    auto temp=battery.getTemperature();
    auto capacity=battery.getCapacity();

    EXPECT_TRUE(voltage.success);
    EXPECT_TRUE(temp.success);
    EXPECT_TRUE(capacity.success);
}
bool run_thread_once=false;

extern void battery_monitor_thread(void);

TEST_F(BatteryFSMTestSuite,MonitorThreadAbortWhenHardwareNotReady){
        mock_device_ready=false;
        testing::internal::CaptureStdout();
        EXPECT_NO_FATAL_FAILURE(battery_monitor_thread());
        std::string output=testing::internal::GetCapturedStdout();
        
        EXPECT_TRUE(output.find("[ERR] I2C hardware not ready. Battery monitor halting.")!= std::string::npos)<<"Thread abort log missing! Actual: "<<output;
}

TEST_F(BatteryFSMTestSuite,ExecutesBatteryMonitorThread){
        mock_battery_current_ma=-500;
        mock_battery_soc_pct=80;
        smart_battery.processFSM();
        
        ASSERT_EQ(smart_battery.getState(), BatteryFSM::DISCHARGING);
        
        testing::internal::CaptureStdout();
        EXPECT_NO_FATAL_FAILURE(battery_monitor_thread());
        std::string output = testing::internal::GetCapturedStdout();
        
        EXPECT_TRUE(output.find("[INF] Battery Discharging: 80% remaining")!=std::string::npos)<<"Thread normal execution log missing! Actual: "<< output;
}

TEST_F(BatteryFSMTestSuite, ThreadLoopConditionAndBranches){
    mock_battery_current_ma=500;
    smart_battery.processFSM();
    
    run_thread_once=true;
    
    testing::internal::CaptureStdout();
    battery_monitor_thread();
    std::string output1=testing::internal::GetCapturedStdout();
    
    EXPECT_TRUE(output1.find("Battery Discharging") == std::string::npos);
    
    mock_battery_current_ma=-500;
    smart_battery.processFSM();
    
    mock_soc_fail=true;
    run_thread_once=false;
    
    testing::internal::CaptureStdout();
    battery_monitor_thread();
    std::string output2 =testing::internal::GetCapturedStdout();
    
    EXPECT_TRUE(output2.find("Battery Discharging: ")==std::string::npos);
}

TEST_F(BatteryFSMTestSuite, RecoveryWhileChargingBranch){
    mock_battery_current_ma=-100;
    mock_battery_soc_pct=10;
    battery.processFSM();
    EXPECT_EQ(battery.getState(),BatteryFSM::CUTOFF);
    EXPECT_EQ(sys_context.getState(), SystemState::SAFE_HALT);
    
    mock_battery_current_ma=500;
    mock_battery_soc_pct=15;
    
    testing::internal::CaptureStdout();
    battery.processFSM();
    std::string output=testing::internal::GetCapturedStdout();
    
    EXPECT_EQ(battery.getState(), BatteryFSM::CHARGING);
    EXPECT_EQ(sys_context.getState(), SystemState::SAFE_HALT);
}
