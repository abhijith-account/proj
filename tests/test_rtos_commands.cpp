#include <gtest/gtest.h>
#include <zephyr/kernel.h>
#include <array>
#include <atomic>
#include <new>
#include <string>

#include "RTOS_Command_based_thread_system.h"
#include "Static_Memory+MISRA_Compliance_Layer.h"
#include "Device_State_Machine+Watchdog.h"
#include "Power_Management_System.h"

bool run_thread_once = false;
DeviceContext sys_context;

extern StaticPool<SensorReadCmd,16> command_pool;
extern struct k_msgq processor_queue;
extern struct k_msgq logger_queue;

extern const k_tid_t producer_tid;
extern const k_tid_t processor_tid;
extern const k_tid_t logger_tid;

extern void processor_thread(void);
extern void logger_thread(void);

std::array<int,2> execution_order;
std::atomic<size_t> exec_index{0};

class PreemptionTestCmd: public ICommand {
      private:
            int thread_priority;
      public:
            PreemptionTestCmd(int prio):thread_priority(prio){}
            
            void execute() override{
                size_t idx=exec_index.fetch_add(1);
                if (idx<execution_order.size()){
                    execution_order[idx]=thread_priority;
                }
            }
};

class StaticDeletionTestCmd : public ICommand{
      public:
          void execute() override{}
          void operator delete(void* ptr){}
};

class RTOSCommandsTestSuite:public::testing::Test{
      protected:
              void SetUp() override{
                  k_msgq_purge(&processor_queue);
                  k_msgq_purge(&logger_queue);
  
                  exec_index=0;
                  execution_order.fill(0);
              }
};

TEST_F(RTOSCommandsTestSuite, BaseCommandCoverage){
    ICommand stack_cmd;
    stack_cmd.execute();

    void* mem=command_pool.allocate();
    ASSERT_NE(mem,nullptr);
    ICommand* dyn_cmd=new (mem) ICommand();
    delete dyn_cmd;
    command_pool.deallocate(mem);
}

TEST_F(RTOSCommandsTestSuite, CommandDispatchAndStaticPoolDestruction){
    void* mem=command_pool.allocate();
    ASSERT_NE(mem,nullptr)<<"StaticPool allocation failed.";
    
    PreemptionTestCmd* mock_cmd=new (mem) PreemptionTestCmd(99);
    ICommand* base_cmd=mock_cmd;
    
    EXPECT_EQ(exec_index.load(),0);
    
    base_cmd->execute();
    EXPECT_EQ(exec_index.load(),1);
    EXPECT_EQ(execution_order[0],99);
    
    base_cmd->destroy();
    
    void* new_mem= command_pool.allocate();
    EXPECT_EQ(new_mem,mem)<<"Memory leak detected: Block not returned to StaticPool.";
    command_pool.deallocate(new_mem);
}

TEST_F(RTOSCommandsTestSuite, MessageQueueLimits){
    ICommand* dummy_cmd_1= reinterpret_cast<ICommand*>(0x1111);
    ICommand* dummy_cmd_2= reinterpret_cast<ICommand*>(0x2222);
    ICommand* overflow_cmd= reinterpret_cast<ICommand*>(0x9999);
    ICommand* out_cmd=nullptr;
    
    EXPECT_EQ(k_msgq_get(&processor_queue, &out_cmd,K_NO_WAIT),-ENOMSG);
    
    EXPECT_EQ(k_msgq_put(&processor_queue,&dummy_cmd_1,K_NO_WAIT),0);
    for (int i=0;i<9;i++){
        EXPECT_EQ(k_msgq_put(&processor_queue,&dummy_cmd_2,K_NO_WAIT),0);
    }
    
    EXPECT_EQ(k_msgq_num_free_get(&processor_queue),0);
    
    EXPECT_EQ(k_msgq_put(&processor_queue,&overflow_cmd,K_NO_WAIT), -ENOMSG);
    
    EXPECT_EQ(k_msgq_get(&processor_queue,&out_cmd,K_NO_WAIT),0);
    EXPECT_EQ(out_cmd,dummy_cmd_1);
    EXPECT_EQ(k_msgq_num_free_get(&processor_queue),1);
}

TEST_F(RTOSCommandsTestSuite, ProductionThreadPreemption){
    EXPECT_EQ(k_thread_priority_get(producer_tid),5);
    EXPECT_EQ(k_thread_priority_get(processor_tid),6);
    EXPECT_EQ(k_thread_priority_get(logger_tid),7);
    
    k_thread_suspend(producer_tid);
    
    k_sched_lock();
    
    int original_prio=k_thread_priority_get(k_current_get());
    k_thread_priority_set(k_current_get(),14);
    
    void* mem2=command_pool.allocate();
    ICommand* cmd2= new (mem2) PreemptionTestCmd(7);
    k_msgq_put(&logger_queue,&cmd2,K_NO_WAIT);
    
    void* mem1=command_pool.allocate();
    ICommand* cmd1= new (mem1) PreemptionTestCmd(6);
    k_msgq_put(&processor_queue,&cmd1,K_NO_WAIT);
    
    k_sched_unlock();
    
    k_msleep(10);
    
    run_thread_once = false;
    processor_thread(); 
    logger_thread();
    
    ASSERT_EQ(exec_index.load(),2);
    
    EXPECT_EQ(execution_order[0],6);
    EXPECT_EQ(execution_order[1],7);
    
    k_thread_priority_set(k_current_get(),original_prio);
    k_thread_resume(producer_tid);
}

TEST_F(RTOSCommandsTestSuite, SensorReadCmdLogsExecution){
    SensorReadCmd cmd;
    testing::internal::CaptureStdout();

    cmd.execute();

    std::string output=testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("[INF] [SensorReadCmd] Executing.")!= std::string::npos)<<"SensorReadCmd execution log missing! Actual: "<<output;
}

TEST_F(RTOSCommandsTestSuite, ComputeCmdLogsProcessingData){
    ComputeCmd cmd(5);
    testing::internal::CaptureStdout();
    
    cmd.execute();
    
    std::string output=testing::internal::GetCapturedStdout(); 
    EXPECT_TRUE(output.find("[INF] [ComputeCmd] Processed 5 -> 500.")!= std::string::npos)<<"ComputeCmd message log missing! Actual:"<<output;
}

TEST_F(RTOSCommandsTestSuite, PrintCmdLogsCustomMessage){
    PrintCmd cmd("System Nominal");
    testing::internal::CaptureStdout();
    
    cmd.execute();
    
    std::string output=testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("[INF] [PrintCmd] Output: System Nominal")!= std::string::npos)<<"PrintCmd message log missing! Actual:"<<output;
}

extern void producer_thread(void);

TEST_F(RTOSCommandsTestSuite,ExecutesProducerThread){
        EXPECT_NO_FATAL_FAILURE(producer_thread());
}

TEST_F(RTOSCommandsTestSuite,ExecutesProducerTHread_QueueFukkDeallocatesMemory){
        ICommand* dummy_cmd=nullptr;
        
        for (int i=0;i<10;i++){
            k_msgq_put(&processor_queue,&dummy_cmd,K_NO_WAIT);
        }
        
        EXPECT_NO_FATAL_FAILURE(producer_thread());
}

TEST_F(RTOSCommandsTestSuite, ProducerHandlesOutOfMemory){
    void* blocks[16];
    for(int i=0;i<16;i++){
        blocks[i]=command_pool.allocate();
    }
    
    run_thread_once=false;
    producer_thread();
    
    for(int i=0;i<16;i++){
        command_pool.deallocate(blocks[i]);
    }
}

TEST_F(RTOSCommandsTestSuite, ProducerSkipsOnSafeHalt){
    sys_context.triggerFault("Coverage Test Halt");

    run_thread_once=false;
    producer_thread();
}

TEST_F(RTOSCommandsTestSuite, ThreadLoopConditionBranches){
    run_thread_once=true;

    void* mem1= command_pool.allocate();
    ICommand* cmd1=new (mem1) PrintCmd("Loop1");
    k_msgq_put(&processor_queue,&cmd1,K_NO_WAIT);
    
    void* mem2= command_pool.allocate();
    ICommand* cmd2 = new (mem2) PrintCmd("Loop2");
    k_msgq_put(&processor_queue,&cmd2,K_NO_WAIT);

    processor_thread();

    run_thread_once=true;
    
    void* mem3 = command_pool.allocate();
    ICommand* cmd3=new (mem3) PrintCmd("Loop3");
    k_msgq_put(&logger_queue,&cmd3,K_NO_WAIT);

    void* mem4 = command_pool.allocate();
    ICommand* cmd4=new (mem4) PrintCmd("Loop4");
    k_msgq_put(&logger_queue,&cmd4,K_NO_WAIT);
    
    logger_thread();
    
    run_thread_once=true;
    
    sys_context=DeviceContext();
    sys_context.requestTransition(SystemState::RUNNING);

    producer_thread();
}
