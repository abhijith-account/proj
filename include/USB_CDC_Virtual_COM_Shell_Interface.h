#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>
#include <array>
#include <string_view>
#include <cstdio>
#include <cstdlib>
#include "Device_State_Machine+Watchdog.h"
#include "Smart_Battery_System.h"
#pragma once

constexpr size_t RX_RING_BUF_SIZE=256;
constexpr size_t MAX_CMD_LEN=128;

class UsbCdcFacade{
    private:
        const device* dev;
        bool dtr_ready;
        std::array<char, RX_RING_BUF_SIZE> rx_buffer;
        volatile size_t rx_head;
        volatile size_t rx_tail;
        
        static void uartInterruptHandler(const device* dev,void* user_data);
        
    public:
        UsbCdcFacade();
        bool init();
        
        bool isConnected();
        
        void transmit(std::string_view data);
        bool readLine(std::array<char, MAX_CMD_LEN>& out_line);
};

class UsbShell{
    private:
        UsbCdcFacade usb;
        DeviceContext* sys_ctx;
        SbsBattery* battery;
        
        void cmdStatus();
        void cmdSetRate(std::string_view args);
        void cmdLogDump();
        void cmdReboot();
        
    public:
        UsbShell(DeviceContext* ctx,SbsBattery* bat);
        void process();
        void dispatchCommand(std::string_view cmd);
};

  

