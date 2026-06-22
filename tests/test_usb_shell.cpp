#include <gtest/gtest.h>
#include <array>
#include <cstring>
#include <string_view>
#include <cstdint>
#include <string>

#include "USB_CDC_Virtual_COM_Shell_Interface.h"
#include "Device_State_Machine+Watchdog.h"
#include "Fault_Tolerant_I2C_Communication_Layer.h"
#include "Smart_Battery_System.h"
#include "Persistent_Configuration_System.h"


#define MOCK_UART_DTR_CTRL 1

static std::array<char,512> mock_tx_buffer;
static size_t mock_tx_index=0;

static std::array<char,512> mock_rx_queue;
static size_t mock_rx_head=0;
static size_t mock_rx_tail=0;
static uint32_t mock_dtr_state=0;

static void (*mock_uart_irq_cb)(const device*, void*)=nullptr;
static void* mock_uart_cb_data=nullptr;

const device* dummy_uart_dev=reinterpret_cast<const device*>(0xCDC);

DeviceContext sys_context;
bool force_device_not_ready = false;
static bool mock_i2c_fail = false;

extern "C" {
    void sys_reboot(int type){}
    
    int wdt_install_timeout(const struct device *dev,const struct wdt_timeout_cfg *cfg){
        return 0;
    }
    int wdt_setup(const struct device *dev,uint8_t options){
        return 0;
    }
    int wdt_feed(const struct device *dev,int channel_id){
        return 0;
    }
    
    int i2c_reg_read_byte(const struct device *dev,uint16_t dev_addr,uint8_t reg_addr,uint8_t *value){
        if (mock_i2c_fail){
            return -1;
        }
        if (value){
            *value=0;
        }
        return 0;
    }
    int i2c_reg_write_byte(const struct device *dev,uint16_t dev_addr,uint8_t reg_addr,uint8_t value){
        return 0;
    }
    int i2c_burst_read(const struct device *dev,uint16_t dev_addr,uint8_t start_addr,uint8_t *buf,uint32_t num_bytes){
        if (mock_i2c_fail){
            return -1;
        }
        return 0;
    }
    static uint8_t mock_nvs_storage[256];
    int nvs_mount(struct nvs_fs *fs){
        return 0;
    }
    ssize_t nvs_read(struct nvs_fs *fs,uint16_t id,void *data,size_t len){
        memcpy(data,&mock_nvs_storage[id],len);
        return 0;
    }
    
    ssize_t nvs_write(struct nvs_fs *fs,uint16_t id,const void *data,size_t len){
        memcpy(&mock_nvs_storage[id],data,len);
        return len;
    }
    
    bool device_is_ready(const struct device *dev) {
        return !force_device_not_ready;
    }
    
    int uart_line_ctrl_get(const struct device *dev,uint32_t ctrl,uint32_t *val){
        if (ctrl == MOCK_UART_DTR_CTRL){
            *val=mock_dtr_state;
        }
        return 0;
    }
    
    void uart_poll_out(const struct device *dev,unsigned char c){
        if (mock_tx_index<mock_tx_buffer.size()){
            mock_tx_buffer[mock_tx_index++]=c;
        }
    }
    
    void uart_irq_callback_user_data_set(const struct device *dev,void (*cb)(const struct device *,void *),void *user_data){
        mock_uart_irq_cb=cb;
        mock_uart_cb_data=user_data;
    }
    
    void uart_irq_rx_enable(const struct device *dev){}
    void uart_irq_update(const struct device *dev){}
    
    int uart_irq_rx_ready(const struct device *dev){
        return (mock_rx_head!=mock_rx_tail)? 1:0;
    }
    
    int uart_fifo_read(const struct device *dev,uint8_t *tx_data,const int size){
        if (mock_rx_head!=mock_rx_tail){
            *tx_data=mock_rx_queue[mock_rx_tail%512];
            mock_rx_tail++;
            return 1;
        }
        return 0;
    } 
}

void inject_mock_uart_data(std::string_view data){
    for (char c: data){
        mock_rx_queue[mock_rx_head%512]=c;
        mock_rx_head++;
    }
    
    if (mock_uart_irq_cb){
        mock_uart_irq_cb(dummy_uart_dev,mock_uart_cb_data);
    }
}

class UsbShellTestSuite:public::testing::Test{
  protected:
      UsbCdcFacade facade;
      
      void SetUp() override{
          mock_dtr_state=0;
          mock_tx_index=0;
          mock_tx_buffer.fill(0);
          mock_rx_head=0;
          mock_rx_tail=0;
          mock_i2c_fail=false;

          facade.init();
      }
};

TEST_F(UsbShellTestSuite,HandlesDTRConnectAndDisconnect){
      mock_dtr_state=0;
      EXPECT_FALSE(facade.isConnected())<<"Facade incorrectly reported connection without DTR high!";
      
      mock_dtr_state=1;
      testing::internal::CaptureStdout();
      EXPECT_TRUE(facade.isConnected())<<"Facade failed to detect DTR connection!";
      std::string output_conn=testing::internal::GetCapturedStdout();
      EXPECT_TRUE(output_conn.find("[INF] USB Terminal Connected (DTR High)")!=std::string::npos)<<"Expected DTR connected log missing!";
      
      
      mock_dtr_state=0;
      testing::internal::CaptureStdout();
      EXPECT_FALSE(facade.isConnected())<<"Facade failed to detect DTR drop!";
      std::string output_disc=testing::internal::GetCapturedStdout();
      EXPECT_TRUE(output_disc.find("[WRN] USB Terminal Disconnected (DTR Low)")!= std::string::npos)<<"Expected DTR drop log missing!";
}

TEST_F(UsbShellTestSuite,InitFailsWhenHardwareNotReady){
      force_device_not_ready=true;
      
      testing::internal::CaptureStdout();
      EXPECT_FALSE(facade.init())<<"Facade failed to abort initialization when hardware was offline!";
      std::string output =testing::internal::GetCapturedStdout();
      
      EXPECT_TRUE(output.find("[ERR] Failed to enable USB")!=std::string::npos)<<"Expected hardware failure log missing!";
      
      force_device_not_ready=false;
}

TEST_F(UsbShellTestSuite,InitSuccessLogsInfo){
    UsbCdcFacade fresh_facade;

    testing::internal::CaptureStdout();
    fresh_facade.init();
    std::string output =testing::internal::GetCapturedStdout();

    EXPECT_TRUE(output.find("[INF] USB CDC Facade Initialized")!=std::string::npos)<<"Expected successful init log missing!";
}

TEST_F(UsbShellTestSuite,TransmitAbortsWhenDisconnected){
    mock_dtr_state=0;
    mock_tx_index=0;
    
    facade.transmit("This should drop");
    
    EXPECT_EQ(mock_tx_index,0)<<"Facade attempted to transmit over a disconnected USB Line";
}

TEST_F(UsbShellTestSuite,ExecuteRemainingCommandsAndLimits){
    DeviceContext sys_ctx;
    I2CManager i2c_mgr{dummy_uart_dev};
    SbsBattery battery{&i2c_mgr, &sys_ctx};
    UsbShell shell(&sys_ctx,&battery);

    mock_dtr_state=1;
    ConfigStore::getInstance().init();

    mock_tx_index=0;
    shell.dispatchCommand("status");
    std::string_view tx_out(mock_tx_buffer.data(),mock_tx_index);
    EXPECT_TRUE(tx_out.find("sys_state")!=std::string_view::npos)<<"Status command failed to output JSON!";

    mock_tx_index=0;    
    shell.dispatchCommand("set_rate 150");
    tx_out=std::string_view(mock_tx_buffer.data(),mock_tx_index);
    EXPECT_TRUE(tx_out.find("Error: Rate must be between 1 and 100")!=std::string_view::npos)<<"Parser failed to reject out-of-bounds rate!";

    mock_tx_index=0;
    shell.dispatchCommand("reboot");
    tx_out=std::string_view(mock_tx_buffer.data(),mock_tx_index);
    EXPECT_TRUE(tx_out.find("Rebooting system...")!=std::string_view::npos)<<"Reboot command failed to execute";
}

TEST_F(UsbShellTestSuite,ReadLineFailsWhenBufferEmpty){
    std::array<char,MAX_CMD_LEN> extracted_cmd;

    EXPECT_FALSE(facade.readLine(extracted_cmd))<<"Facade returned true on an empty ring buffer!";
}

TEST_F(UsbShellTestSuite,RingBufferWrapAroundAndLineDecoding){
    std::array<char,MAX_CMD_LEN> extracted_cmd;
    
    std::string batch1(100,'A');
    batch1+="\n";
    inject_mock_uart_data(batch1);
    
    EXPECT_TRUE(facade.readLine(extracted_cmd));
    EXPECT_STREQ(extracted_cmd.data(),std::string(100,'A').c_str());
    
    std::string batch2(100,'B');
    batch2+="\n";
    inject_mock_uart_data(batch2);
    
    EXPECT_TRUE(facade.readLine(extracted_cmd));
    EXPECT_STREQ(extracted_cmd.data(),std::string(100,'B').c_str());
    
    std::string batch3(100,'C');
    batch3+="\n";
    inject_mock_uart_data(batch3);
    
    EXPECT_TRUE(facade.readLine(extracted_cmd));
    EXPECT_STREQ(extracted_cmd.data(),std::string(100,'C').c_str())<<"Ring buffer modulo math corrupted the command during wrap-around!";
}

TEST_F(UsbShellTestSuite, IgnoresIncompleteCommands){
    std::array<char,MAX_CMD_LEN> extracted_cmd;

    inject_mock_uart_data("set_rate 50");

    bool read_success=facade.readLine(extracted_cmd);
    EXPECT_FALSE(read_success)<<"Facade prematurely decoded an incomplete command!";
    
    inject_mock_uart_data("\n");

    read_success=facade.readLine(extracted_cmd);
    EXPECT_TRUE(read_success);
    EXPECT_STREQ(extracted_cmd.data(),"set_rate 50");
}

TEST_F(UsbShellTestSuite, DecodeCLICommands){
    DeviceContext sys_ctx;
    I2CManager i2c_mgr{dummy_uart_dev};
    SbsBattery battery{&i2c_mgr,&sys_ctx};
    UsbShell shell(&sys_ctx,&battery);
    
    mock_dtr_state=1;
    ConfigStore::getInstance().init();

    mock_tx_index=0;
    shell.dispatchCommand("set_rate 85");
    
    std::string_view tx_out(mock_tx_buffer.data(),mock_tx_index);
    EXPECT_TRUE(tx_out.find("85")!=std::string_view::npos)<<"Parser failed to decode 'set-rate' command!";
    
    uint16_t saved_rate=0;
    ConfigStore::getInstance().get(ConfigKey::INFUSION_RATE,saved_rate);
    EXPECT_EQ(saved_rate,85)<<"Parser failed to extract the integer 85 from the string!";
    
    mock_tx_index=0;
    shell.dispatchCommand("log dump");
    
    tx_out=std::string_view(mock_tx_buffer.data(),mock_tx_index);
    EXPECT_TRUE(tx_out.find("System Log Dump")!=std::string_view::npos);
    
    mock_tx_index=0;
    shell.dispatchCommand("set_rate_85");
    
    tx_out=std::string_view(mock_tx_buffer.data(),mock_tx_index);
    EXPECT_TRUE(tx_out.find("Error: Unknown command")!=std::string_view::npos)<<"Parser failed to catch and reject an invalid command!";
}
bool run_thread_once=false;

extern void shell_thread(void);

TEST_F(UsbShellTestSuite,ShellThread){
        mock_dtr_state=1;
        EXPECT_NO_FATAL_FAILURE(shell_thread());
        inject_mock_uart_data("status\n");
        EXPECT_NO_FATAL_FAILURE(shell_thread());
}

TEST_F(UsbShellTestSuite, StatusCommandWithSoCFailure){
    DeviceContext sys_ctx;
    I2CManager i2c_mgr{dummy_uart_dev};
    SbsBattery battery{&i2c_mgr,&sys_ctx};
    UsbShell shell(&sys_ctx,&battery); 

    mock_dtr_state=1;
    mock_i2c_fail=true;
    
    mock_tx_index=0;
    shell.dispatchCommand("status");

    std::string_view tx_out(mock_tx_buffer.data(),mock_tx_index);
    EXPECT_TRUE(tx_out.find("\"battery_soc\":0")!=std::string_view::npos);
}

TEST_F(UsbShellTestSuite,RingBufferOverflow){
    UsbCdcFacade local_facade;
    local_facade.init();
    
    std::string massive_payload(300,'X');
    inject_mock_uart_data(massive_payload);
    
    SUCCEED();
}

TEST_F(UsbShellTestSuite, ShellThreadEmptyCommandAndLoopback){
    run_thread_once=false;
    shell_thread();
    
    inject_mock_uart_data("\n");

    run_thread_once=true;
    mock_dtr_state=1;
    shell_thread();
}

TEST_F(UsbShellTestSuite, SpuriousIrqWakeup){
    mock_rx_head=0;
    mock_rx_tail=0;
    
    if (mock_uart_irq_cb){
        mock_uart_irq_cb(dummy_uart_dev, mock_uart_cb_data);
    }
    SUCCEED();
}

TEST_F(UsbShellTestSuite, ReadLineMaxLengthTruncation){
    std::array<char,MAX_CMD_LEN> extracted_cmd;
    
    std::string long_cmd(150,'A');
    inject_mock_uart_data(long_cmd);

    EXPECT_FALSE(facade.readLine(extracted_cmd));
}

TEST_F(UsbShellTestSuite, ReadLineHandlesCarriageReturn){
    std::array<char,MAX_CMD_LEN> extracted_cmd;
    
    inject_mock_uart_data("status\r");
    
    EXPECT_TRUE(facade.readLine(extracted_cmd));
    EXPECT_STREQ(extracted_cmd.data(),"status");
}

TEST_F(UsbShellTestSuite,SetRateRejectsZeroOrNegative){
    DeviceContext sys_ctx;
    I2CManager i2c_mgr{dummy_uart_dev};
    SbsBattery battery{&i2c_mgr,&sys_ctx};
    UsbShell shell(&sys_ctx,&battery);

    mock_dtr_state=1;
    mock_tx_index=0;

    shell.dispatchCommand("set_rate 0");

    std::string_view tx_out(mock_tx_buffer.data(),mock_tx_index);
    EXPECT_TRUE(tx_out.find("Error: Rate must be between 1 and 100")!=std::string_view::npos);
}

