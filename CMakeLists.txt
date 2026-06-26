cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(medical_firmware)

option(QEMU_RUNTIME_PROBE "Enable QEMU runtime probe markers and hardware stubs" OFF)

target_include_directories(app PRIVATE include)

target_sources(app PRIVATE
    src/main.cpp
    src/RTOS_Command_based_thread_system.cpp
    src/RTOS_Synchronization_Layer.cpp
)

if(QEMU_RUNTIME_PROBE)
    # This block now automatically defines the macros your C++ code expects
    target_compile_definitions(app PRIVATE 
        QEMU_RUNTIME_PROBE=1
        CONFIG_QEMU_CORTEX_M3=1
        CONFIG_QEMU_CORTEX_M4=1 
    )

    target_sources(app PRIVATE
        src/QEMU_Startup_Probe.c
        src/Device_State_Machine_QEMU_Stub.cpp
        src/Persistent_Configuration_System_QEMU_Stub.cpp
        src/Power_Management_System_QEMU_Stub.cpp
        src/Smart_Battery_System_QEMU_Stub.cpp
        src/USB_CDC_Virtual_COM_Shell_Interface_QEMU_Stub.cpp
        src/Fault_Tolerant_I2C_Communication_Layer_QEMU_Stub.cpp
        src/Static_Memory_QEMU_Stub.cpp
    )
else()
    target_sources(app PRIVATE
        src/Device_State_Machine+Watchdog.cpp
        src/Fault_Tolerant_I2C_Communication_Layer.cpp
        src/Persistent_Configuration_System.cpp
        src/Power_Management_System.cpp
        src/Smart_Battery_System.cpp
        src/Static_Memory+MISRA_Compliance_Layer.cpp
        src/USB_CDC_Virtual_COM_Shell_Interface.cpp
    )
endif()

target_compile_options(app PRIVATE
    -Wall
    -Wextra
    -Werror
    -Wno-pedantic
    -Wno-variadic-macros
    -Wno-vla
    -Wno-missing-field-initializers
    -Wno-unused-parameter
)

if(CONFIG_COVERAGE)
    target_compile_options(app PRIVATE --coverage -O0)
    target_link_options(app PRIVATE --coverage)
endif()
