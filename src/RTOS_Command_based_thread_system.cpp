#include <zephyr/kernel.h>
#include "RTOS_Command_based_thread_system.h"
#include <zephyr/logging/log.h>
#include "Static_Memory+MISRA_Compliance_Layer.h"
#include "Device_State_Machine+Watchdog.h"
#include "Power_Management_System.h"

#ifdef IS_TEST_ENVIRONMENT
    extern bool run_thread_once;
    #define THREAD_LOOP_CONDITION (run_thread_once ? (run_thread_once = false, true) : false)
#else
    #define THREAD_LOOP_CONDITION true
#endif

LOG_MODULE_REGISTER(COMMANDS, LOG_LEVEL_INF);

StaticPool<SensorReadCmd,16> command_pool;
extern DeviceContext sys_context;
extern PowerManager pwr_manager;

K_MSGQ_DEFINE(processor_queue, sizeof(ICommand*),10,4);
K_MSGQ_DEFINE(logger_queue, sizeof(ICommand*),10,4);

ICommand::ICommand(){
    timestamp_queued=k_cycle_get_32();
}

void ICommand::destroy(){
    this->~ICommand();
    command_pool.deallocate(this);
}

void SensorReadCmd::execute(){
    uint32_t delay=k_cycle_get_32()-timestamp_queued;
    LOG_INF("[SensorReadCmd] Executing. Queue Delay: %u cycles",delay);
}

ComputeCmd::ComputeCmd(uint8_t data):raw_data(data){}

void ComputeCmd::execute(){
    uint32_t delay=k_cycle_get_32()-timestamp_queued;
    uint32_t processed =raw_data*100;
    LOG_INF("[ComputeCmd] Processed %u -> %u. Delay: %u",raw_data,processed,delay);
}

PrintCmd::PrintCmd(const char* msg):message(msg){}

void PrintCmd::execute(){
    LOG_INF("[PrintCmd] Output: %s", message);
}

void producer_thread(void){
    do{
       if (sys_context.getState()!=SystemState::SAFE_HALT){
          void* mem=command_pool.allocate();
          
          if (mem!=nullptr){
              ICommand* cmd= new (mem) SensorReadCmd();
              if (k_msgq_put(&processor_queue, &cmd,K_NO_WAIT)!=0){
                    cmd->~ICommand();
                    command_pool.deallocate(mem);
              }
              else{
                  pwr_manager.reportActivity();
              }
          }   
       }  
    k_msleep(100); 
    }while(THREAD_LOOP_CONDITION);
}

void processor_thread(void){
    ICommand* incoming_cmd;
    do{
        k_msgq_get(&processor_queue, &incoming_cmd,K_FOREVER);
        incoming_cmd->execute();
        
        void* raw_mem=incoming_cmd;
        incoming_cmd->~ICommand();
        command_pool.deallocate(raw_mem);
    }while(THREAD_LOOP_CONDITION);
}

void logger_thread(void){
    ICommand* incoming_cmd;
    do{
        k_msgq_get(&logger_queue, &incoming_cmd, K_FOREVER);
        incoming_cmd->execute();
        
        void* raw_mem=incoming_cmd;
        incoming_cmd->~ICommand();
        command_pool.deallocate(raw_mem);
    }while(THREAD_LOOP_CONDITION);
}

K_THREAD_DEFINE(producer_tid,1024,producer_thread,NULL,NULL,NULL,5,0,0);
K_THREAD_DEFINE(processor_tid,1024,processor_thread,NULL,NULL,NULL,6,0,0);
K_THREAD_DEFINE(logger_tid,1024,logger_thread,NULL,NULL,NULL,7,0,0);
