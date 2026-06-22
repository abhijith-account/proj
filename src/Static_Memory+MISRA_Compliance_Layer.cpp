#include <zephyr/kernel.h>
#include "Static_Memory+MISRA_Compliance_Layer.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/debug/thread_analyzer.h>
#ifdef IS_TEST_ENVIRONMENT
    extern bool run_thread_once;
    #define THREAD_LOOP_CONDITION (run_thread_once ? (run_thread_once = false, true) : false)
#else 
    #define THREAD_LOOP_CONDITION true
#endif

void memory_monitor_thread(void){
    do{
    LOG_INF("=== [System Health] Thread Stack Watermarks ===");
    thread_analyzer_print();
    k_msleep(5000);
    }while(THREAD_LOOP_CONDITION);
}

K_THREAD_DEFINE(mem_mon_tid,1024,memory_monitor_thread,NULL,NULL,NULL,11,0,0);
