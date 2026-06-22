#include <zephyr/kernel.h>
#include <cstdint>
#pragma once

class ICommand{
  public:
    uint32_t timestamp_queued;
    
    ICommand();
    virtual ~ICommand()=default;
    
    virtual void execute() {} 
    
    void operator delete(void* ptr) {}
    
    void destroy();
};

class SensorReadCmd:public ICommand{
  public:
      void execute() override;
};

class ComputeCmd:public ICommand{
  private:
      uint8_t raw_data;
  public:
      explicit ComputeCmd(uint8_t data);
      void execute() override;
};

class PrintCmd:public ICommand{
  private:
      const char* message;
  public:
      explicit PrintCmd(const char* msg);
      void execute() override;
};

extern struct k_msgq processor_queue;
extern struct k_msgq logger_queue;    
