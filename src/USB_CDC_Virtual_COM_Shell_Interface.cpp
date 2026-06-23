#include "USB_CDC_Virtual_COM_Shell_Interface.h"
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include "Persistent_Configuration_System.h"
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>
#ifdef IS_TEST_ENVIRONMENT
    extern bool run_thread_once;
    #define THREAD_LOOP_CONDITION (run_thread_once ? (run_thread_once = false, true) : false)
#else
    #define THREAD_LOOP_CONDITION true
#endif

LOG_MODULE_REGISTER(USB_CLI,LOG_LEVEL_INF);

UsbCdcFacade::UsbCdcFacade():dtr_ready(false),rx_head(0),rx_tail(0){
      dev = DEVICE_DT_GET(DT_ALIAS(cdc_acm_uart0));
}

extern DeviceContext sys_context;
extern SbsBattery smart_battery;
UsbShell diag_shell(&sys_context, &smart_battery);

bool UsbCdcFacade::init(){
    if (!device_is_ready(dev)){
        LOG_ERR("Failed to enable USB");    
        return false;
    }
    
    uart_irq_callback_user_data_set(dev,uartInterruptHandler,this);
    uart_irq_rx_enable(dev);
    
    LOG_INF("USB CDC Facade Initialized");
    return true;
}

bool UsbCdcFacade::isConnected(){
    uint32_t dtr=0;
    uart_line_ctrl_get(dev,UART_LINE_CTRL_DTR,&dtr);
    
    bool currently_connected=(dtr!=0);
    if (currently_connected && !dtr_ready){
      LOG_INF("USB Terminal Connected (DTR High)");
    }
    else if (!currently_connected && dtr_ready){
      LOG_WRN("USB Terminal Disconnected (DTR Low)");
    }
    dtr_ready=currently_connected;
    return dtr_ready;
}

void UsbCdcFacade::transmit(std::string_view data){
    if (!isConnected()){
        return;
    }
    
    for (char c : data){
        uart_poll_out(dev,c);
    }
}

void UsbCdcFacade::uartInterruptHandler(const device* dev,void* user_data){
    auto* self=static_cast<UsbCdcFacade*>(user_data);
    uart_irq_update(dev);
    
    if (uart_irq_rx_ready(dev)){
        uint8_t c;
        while (uart_fifo_read(dev,&c,1)==1){
            size_t next_head=(self->rx_head+1) % RX_RING_BUF_SIZE;
            if (next_head!= self->rx_tail){
                self->rx_buffer[self->rx_head]=static_cast<char>(c);
                self->rx_head=next_head;
            }
        }
    }
}

bool UsbCdcFacade::readLine(std::array<char,MAX_CMD_LEN>& out_line){
    if (rx_head==rx_tail){
      return false;
    }
    
    size_t temp_tail=rx_tail;
    size_t i=0;
    bool found_eol=false;

    while (temp_tail!=rx_head && i< MAX_CMD_LEN -1){
        char c=rx_buffer[temp_tail];
        if (c=='\r' || c=='\n'){
            found_eol=true;
            temp_tail=(temp_tail+1) % RX_RING_BUF_SIZE; 
            break;
        }
        out_line[i++]=c;
        temp_tail=(temp_tail+1) % RX_RING_BUF_SIZE;
    }
    
    if (found_eol){
        out_line[i]='\0';
        rx_tail=temp_tail;
        return true;
    }
    return false;
}

UsbShell::UsbShell(DeviceContext* ctx,SbsBattery* bat):sys_ctx(ctx),battery(bat){}

void UsbShell::process(){
    usb.init();
    std::array<char, MAX_CMD_LEN> cmd_buf;
    
    do{
        if (usb.isConnected() && usb.readLine(cmd_buf)){
            if (cmd_buf[0]!='\0'){
               dispatchCommand(std::string_view(cmd_buf.data()));
               usb.transmit("med-device:~$");
            }
         }
         k_msleep(50);
    }while(THREAD_LOOP_CONDITION);
}

void UsbShell::dispatchCommand(std::string_view cmd){
    if (cmd=="status"){
        cmdStatus();
    }
    else if (cmd.starts_with("set_rate ")){
        cmdSetRate(cmd.substr(9));
    }
    else if (cmd=="log dump"){
        cmdLogDump();
    }
    else if (cmd=="reboot"){
        cmdReboot();
    }
    else{
        usb.transmit("Error: Unknown command.\r\n");
    }
}

void UsbShell::cmdStatus(){
    int state_val=static_cast<int>(sys_ctx->getState());
    uint8_t soc=0;
    auto soc_res=battery->getStateOfCharge();
    if (soc_res.success){
        soc=soc_res.value;
    }
    
    uint16_t rate=0;
    ConfigStore::getInstance().get(ConfigKey::INFUSION_RATE,rate);
    
    std::array<char,128> json_stack_buf;
    
    snprintf(json_stack_buf.data(),json_stack_buf.size(),"{\"sys_state\":%d,\"battery_soc\":%u,\"infusion_rate\":%u}\r\n",state_val,soc,rate);
    
    usb.transmit(json_stack_buf.data());
}

void UsbShell::cmdSetRate(std::string_view args){
    int rate=std::atoi(args.data());

    if (rate>=1 && rate<=100){
        ConfigStore::getInstance().set(ConfigKey::INFUSION_RATE, static_cast<uint16_t>(rate));
        
        std::array<char, 64> msg;
        
        snprintf(msg.data(),msg.size(),"Success: Rate set to be %d mL/hr\r\n",rate);
        usb.transmit(msg.data());
    }
    else{
        usb.transmit("Error: Rate must be between 1 and 100.\r\n");
    }
}

void UsbShell::cmdLogDump(){
    usb.transmit("--- NVS System Log Dump ---\r\n");
    usb.transmit("System Booted\r\n");
    usb.transmit("I2C Manager Initialized\r\n");
    usb.transmit("--- End of Logs ---\r\n");
}

void UsbShell::cmdReboot(){
    usb.transmit("Rebooting system... \r\n");
    k_msleep(100);
    sys_reboot(SYS_REBOOT_COLD);
}

void shell_thread(void){
    diag_shell.process();
}

K_THREAD_DEFINE(shell_tid,1024,shell_thread,NULL,NULL,NULL,12,0,0);

