#include <gtest/gtest.h>
#include <string>
#include "Device_State_Machine+Watchdog.h"

static bool mock_wdt_ready=true;
static int mock_wdt_install_res=0;

extern "C" {
    bool device_is_ready(const struct device *dev){
        return mock_wdt_ready;
    }
    
    int wdt_install_timeout(const struct device *dev,const struct wdt_timeout_cfg *cfg){
        return mock_wdt_install_res;
    }
    
    int wdt_setup(const struct device *dev,uint8_t options){
        return 0;
    }
    
    int wdt_feed(const struct device *dev,int channel_id){
        return 0;
    }
}

class StateMachineTestSuite:public::testing::Test{
      protected:
            void SetUp() override{
                mock_wdt_ready=true;
                mock_wdt_install_res=0;
            }
};

TEST_F(StateMachineTestSuite, AllowsLegalTransitions){
    DeviceContext sys_context;

    EXPECT_EQ(sys_context.getState(),SystemState::INIT)<<"System did not boot into INIT state";
    
    testing::internal::CaptureStdout();
    bool success_run=sys_context.requestTransition(SystemState::RUNNING);
    std::string output_run=testing::internal::GetCapturedStdout();

    
    EXPECT_TRUE(success_run)<<"Valid transition to RUNNING was rejected";
    EXPECT_EQ(sys_context.getState(),SystemState::RUNNING);
    EXPECT_TRUE(output_run.find("[INF] System transitioned to state:")!=std::string::npos)<<"Expected transition INF log missing!";
    
    testing::internal::CaptureStdout();
    bool success_fault=sys_context.requestTransition(SystemState::FAULT);
    std::string output_fault=testing::internal::GetCapturedStdout();
    
    EXPECT_TRUE(success_fault)<<"Valid transition from RUNNING to FAULT was rejected";
    EXPECT_EQ(sys_context.getState(),SystemState::FAULT);
    EXPECT_TRUE(output_fault.find("[INF] System transitioned to state:")!=std::string::npos)<<"Expected transition INF log missing";
}

TEST_F(StateMachineTestSuite, FaultInjectionTriggersSafeHalt){
    DeviceContext sys_context;

    sys_context.requestTransition(SystemState::RUNNING);
    ASSERT_EQ(sys_context.getState(),SystemState::RUNNING);
    
    testing::internal::CaptureStdout();
    sys_context.triggerFault("Simulated I2C Bus Hard-Lock");
    std::string output=testing::internal::GetCapturedStdout();
    
    EXPECT_EQ(sys_context.getState(),SystemState::SAFE_HALT)<<"System failed to drop to SAFE_HALT after a critical fault!";
    EXPECT_TRUE(output.find("[ERR] CRITICAL FAULT: Simulated I2C Bus Hard-Lock. Forcing SAFE_HALT")!= std::string::npos)<<"Expected critical fault log missing! Actual output:  "<< output;
}

TEST_F(StateMachineTestSuite,RejectsIllegalTransitions){
    DeviceContext sys_context_halted;
    
    sys_context_halted.triggerFault("Test Error");
    ASSERT_EQ(sys_context_halted.getState(),SystemState::SAFE_HALT);

    testing::internal::CaptureStdout();
    bool success_halt=sys_context_halted.requestTransition(SystemState::RUNNING);
    std::string output_halt_reject=testing::internal::GetCapturedStdout();
    
    EXPECT_FALSE(success_halt)<<"System allowed an unsafe transition out of SAFE_HALT!";
    EXPECT_EQ(sys_context_halted.getState(),SystemState::SAFE_HALT)<<"System state inappropriately changed from SAFE_HALT!";
    EXPECT_TRUE(output_halt_reject.find("[ERR] Cannot transition out of FAULT/HALT state normally.")!= std::string::npos)<<"Expected rejection error log missing!";
    
    DeviceContext sys_context_fault;
    
    sys_context_fault.requestTransition(SystemState::FAULT);
    ASSERT_EQ(sys_context_fault.getState(),SystemState::FAULT);
    
    testing::internal::CaptureStdout();
    bool success_fault = sys_context_fault.requestTransition(SystemState::RUNNING);
    std::string output_fault_reject =testing::internal::GetCapturedStdout();
    
    EXPECT_FALSE(success_fault)<<"System state allowed transition out of FAULT without a reboot!";
    EXPECT_EQ(sys_context_fault.getState(),SystemState::FAULT);
    EXPECT_TRUE(output_fault_reject.find("[ERR] Cannot transition out of FAULT/HALT state normally.")!= std::string::npos)<<"Expected rejection error log missing!";
}

TEST_F(StateMachineTestSuite, WatchdogInitSuccess){
  WatchdogTimer wdt;
  EXPECT_TRUE(wdt.init(1000));

  EXPECT_NO_FATAL_FAILURE(wdt.feed());
}

TEST_F(StateMachineTestSuite, WatchdogInitFailsDeviceNotReady){
    WatchdogTimer wdt;
    mock_wdt_ready=false;

    EXPECT_FALSE(wdt.init(1000));

    EXPECT_NO_FATAL_FAILURE(wdt.feed());
}

TEST_F(StateMachineTestSuite, WatchdogInitFailsInstall){
    WatchdogTimer wdt;
    mock_wdt_install_res=-1;

    EXPECT_FALSE(wdt.init(1000));
}
