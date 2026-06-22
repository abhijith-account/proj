cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(medical_firmware)

zephyr_cc_option(-Wall -Wextra -Werror)
zephyr_cxx_option(-Wall -Wextra -Werror -pedantic)

if(CONFIG_COVERAGE)
	zephyr_cc_option(--coverage -O0)
	zephyr_cxx_option(--coverage)
endif()	

target_include_directories(app PRIVATE include)

target_sources(app PRIVATE 
    src/main.cpp
    src/Device_State_Machine+Watchdog.cpp
    src/Fault_Tolerant_I2C_Communication_Layer.cpp
    src/Persistent_Configuration_System.cpp
    src/Power_Management_System.cpp
    src/RTOS_Command_based_thread_system.cpp
    src/RTOS_Synchronization_Layer.cpp
    src/Smart_Battery_System.cpp
    src/Static_Memory+MISRA_Compliance_Layer.cpp
    src/USB_CDC_Virtual_COM_Shell_Interface.cpp
)
