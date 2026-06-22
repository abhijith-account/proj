cmake_minimum_required(VERSION 3.10)
project(pm_tests)

set(CMAKE_CXX_STANDARD 20)

# Find Google Test on your host system
find_package(GTest REQUIRED)
set(MOCK_DIR ${CMAKE_BINARY_DIR}/zephyr_mocks)
file(MAKE_DIRECTORY ${MOCK_DIR}/zephyr/pm)
file(MAKE_DIRECTORY ${MOCK_DIR}/zephyr/drivers)
file(MAKE_DIRECTORY ${MOCK_DIR}/zephyr/logging)

# 2. Generate the stub headers to bypass the real OS
file(WRITE ${MOCK_DIR}/zephyr/kernel.h "#pragma once\n#include <stdint.h>\n#include <stddef.h>\n#include <string.h>\n#include <errno.h>\n\ntypedef int32_t k_timeout_t;\n#define K_NO_WAIT 0\n#define K_MSEC(ms) (ms)\n#define K_SECONDS(s) ((s)*1000)\n#ifndef EBUSY\n#define EBUSY 16\n#endif\n#ifndef ENOMSG\n#define ENOMSG 42\n#endif\n\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n\nextern uint32_t virtual_uptime;\nstatic inline uint32_t k_uptime_get_32(void) { return virtual_uptime; }\nstatic inline uint32_t k_cycle_get_32(void) { return virtual_uptime * 1000; }\nstatic inline int k_msleep(int ms) { virtual_uptime += ms; return 0; }\n\nstruct k_mutex { int locked; };\nstruct k_sem { unsigned int count; unsigned int limit; };\nstruct k_work { void* dummy; };\nstruct k_work_delayable { struct k_work work; };\n\n#define K_FOREVER -1\n\nstatic inline int k_mutex_init(struct k_mutex *mutex) { return 0; }\nstatic inline int k_mutex_lock(struct k_mutex *mutex, int timeout) { return 0; }\nstatic inline int k_mutex_unlock(struct k_mutex *mutex) { return 0; }\n\nint k_sem_init(struct k_sem *sem, unsigned int initial, unsigned int limit);\nvoid k_sem_give(struct k_sem *sem);\nint k_sem_take(struct k_sem *sem, k_timeout_t timeout);\nvoid k_work_init_delayable(struct k_work_delayable *dwork, void (*callback)(struct k_work *));\nint k_work_schedule(struct k_work_delayable *dwork, k_timeout_t delay);\n\ntypedef uintptr_t k_tid_t;\nstatic inline int k_thread_priority_get(k_tid_t thread) { return (int)thread; }\nstatic inline void k_thread_priority_set(k_tid_t thread, int prio) {}\nstatic inline void k_thread_suspend(k_tid_t thread) {}\nstatic inline void k_thread_resume(k_tid_t thread) {}\nstatic inline void k_sched_lock(void) {}\nstatic inline void k_sched_unlock(void) {}\nstatic inline k_tid_t k_current_get(void) { return 0; }\n\nstruct k_msgq { size_t msg_size; uint32_t max_msgs; uint32_t num_msgs; void* buffer; };\n#define K_MSGQ_DEFINE(name, q_msg_size, q_max_msgs, q_align) static char __msgq_buf_##name[(q_msg_size) * (q_max_msgs)]; struct k_msgq name = { .msg_size = (q_msg_size), .max_msgs = (q_max_msgs), .num_msgs = 0, .buffer = __msgq_buf_##name };\nstatic inline int k_msgq_put(struct k_msgq *q, const void *data, k_timeout_t timeout) { if(q->num_msgs >= q->max_msgs) return -ENOMSG; memcpy((char*)q->buffer + (q->num_msgs * q->msg_size), data, q->msg_size); q->num_msgs++; return 0; }\nstatic inline int k_msgq_get(struct k_msgq *q, void *data, k_timeout_t timeout) { if(q->num_msgs == 0) return -ENOMSG; memcpy(data, q->buffer, q->msg_size); q->num_msgs--; memmove(q->buffer, (char*)q->buffer + q->msg_size, q->num_msgs * q->msg_size); return 0; }\nstatic inline void k_msgq_purge(struct k_msgq *q) { q->num_msgs = 0; }\nstatic inline uint32_t k_msgq_num_free_get(struct k_msgq *q) { return q->max_msgs - q->num_msgs; }\n\n#ifdef __cplusplus\n}\n#endif\n\n#ifndef CONTAINER_OF\n#define CONTAINER_OF(ptr, type, field) ((type *)(((char *)(ptr)) - offsetof(type, field)))\n#endif\n\n#ifndef k_work_delayable_from_work\n#define k_work_delayable_from_work(w) CONTAINER_OF(w, struct k_work_delayable, work)\n#endif\n\n#ifndef K_THREAD_DEFINE\n#define K_THREAD_DEFINE(name, stack_size, entry, p1, p2, p3, prio, options, delay) extern const k_tid_t name; const k_tid_t name = (k_tid_t)prio;\n#endif\n")
file(WRITE ${MOCK_DIR}/zephyr/device.h "#pragma once\nstruct device {};\n#define DEVICE_DT_GET(...) nullptr\n#define DEVICE_DT_GET_ONE(...) nullptr\n#define DT_NODELABEL(...) 0\nextern \"C\" bool device_is_ready(const struct device *dev);\n")
file(WRITE ${MOCK_DIR}/zephyr/logging/log.h "#pragma once\n#include <stdio.h>\n#define LOG_MODULE_REGISTER(...)\n#define LOG_INF(fmt, ...) do { printf(\"[INF] \" fmt \"\\n\", ##__VA_ARGS__); fflush(stdout); } while(0)\n#define LOG_WRN(fmt, ...) do { printf(\"[WRN] \" fmt \"\\n\", ##__VA_ARGS__); fflush(stdout); } while(0)\n#define LOG_ERR(fmt, ...) do { printf(\"[ERR] \" fmt \"\\n\", ##__VA_ARGS__); fflush(stdout); } while(0)\n")
file(WRITE ${MOCK_DIR}/zephyr/pm/pm.h "#pragma once\n#define PM_STATE_SUSPEND_TO_RAM 1\n#define PM_ALL_SUBSTATES 0\n")
file(WRITE ${MOCK_DIR}/zephyr/pm/policy.h "#pragma once\n#include <stdint.h>\nextern \"C\" void pm_policy_state_lock_get(uint8_t state, uint8_t substate_id);\nextern \"C\" void pm_policy_state_lock_put(uint8_t state, uint8_t substate_id);\n")
file(WRITE ${MOCK_DIR}/zephyr/pm/device.h "#pragma once\nenum pm_device_action { PM_DEVICE_ACTION_SUSPEND, PM_DEVICE_ACTION_RESUME };\nextern \"C\" int pm_device_action_run(const struct device *dev, enum pm_device_action action);\n")
file(WRITE ${MOCK_DIR}/zephyr/drivers/counter.h "#pragma once\n#include <stdint.h>\nstruct counter_alarm_cfg { uint32_t flags; uint32_t ticks; void (*callback)(const struct device *, uint8_t, uint32_t, void *); void *user_data; };\nextern \"C\" uint32_t counter_us_to_ticks(const struct device *dev, uint64_t us);\nextern \"C\" int counter_set_channel_alarm(const struct device *dev, uint8_t chan_id, const struct counter_alarm_cfg *alarm_cfg);\n")
file(WRITE ${MOCK_DIR}/zephyr/drivers.h "#pragma once\n")
file(WRITE ${MOCK_DIR}/zephyr/drivers/uart.h "#pragma once\n#include <stdint.h>\n#define UART_LINE_CTRL_DTR 1\nextern \"C\" {\nint uart_line_ctrl_get(const struct device *dev, uint32_t ctrl, uint32_t *val);\nvoid uart_poll_out(const struct device *dev, unsigned char c);\nvoid uart_irq_callback_user_data_set(const struct device *dev, void (*cb)(const struct device *, void *), void *user_data);\nvoid uart_irq_rx_enable(const struct device *dev);\nvoid uart_irq_update(const struct device *dev);\nint uart_irq_rx_ready(const struct device *dev);\nint uart_fifo_read(const struct device *dev, uint8_t *tx_data, const int size);\n}\n")
file(MAKE_DIRECTORY ${MOCK_DIR}/zephyr/usb)
file(WRITE ${MOCK_DIR}/zephyr/usb/usb_device.h "#pragma once\nextern \"C\" int usb_enable(void *status_cb);\n")
file(WRITE ${MOCK_DIR}/zephyr/drivers/watchdog.h "#pragma once\n#include <stdint.h>\n#define WDT_FLAG_RESET_SOC 1\n#define WDT_OPT_PAUSE_HALTED_BY_DBG 1\nstruct wdt_window { uint32_t min; uint32_t max; };\nstruct wdt_timeout_cfg { struct wdt_window window; void (*callback)(const struct device *, int); uint8_t flags; };\nextern \"C\" {\nint wdt_install_timeout(const struct device *dev, const struct wdt_timeout_cfg *cfg);\nint wdt_setup(const struct device *dev, uint8_t options);\nint wdt_feed(const struct device *dev, int channel_id);\n}\n")
file(WRITE ${MOCK_DIR}/zephyr/drivers/i2c.h "#pragma once\n#include <stdint.h>\n#include <stddef.h>\nextern \"C\" {\nint i2c_read(const struct device *dev, uint8_t *buf, uint32_t num_bytes, uint16_t addr);\nint i2c_write(const struct device *dev, const uint8_t *buf, uint32_t num_bytes, uint16_t addr);\nint i2c_write_read(const struct device *dev, uint16_t addr, const void *write_buf, size_t num_write, void *read_buf, size_t num_read);\nint i2c_reg_read_byte(const struct device *dev, uint16_t dev_addr, uint8_t reg_addr, uint8_t *value);\nint i2c_reg_write_byte(const struct device *dev, uint16_t dev_addr, uint8_t reg_addr, uint8_t value);\nint i2c_burst_read(const struct device *dev, uint16_t dev_addr, uint8_t start_addr, uint8_t *buf, uint32_t num_bytes);\n}\n")
file(WRITE ${MOCK_DIR}/zephyr/drivers/flash.h "#pragma once\n#include <sys/types.h>\nextern \"C\" {\nint flash_read(const struct device *dev, off_t offset, void *data, size_t len);\nint flash_write(const struct device *dev, off_t offset, const void *data, size_t len);\nint flash_erase(const struct device *dev, off_t offset, size_t size);\n}\n")
file(MAKE_DIRECTORY ${MOCK_DIR}/zephyr/fs)
file(WRITE ${MOCK_DIR}/zephyr/fs/nvs.h "#pragma once\n#include <stdint.h>\n#include <sys/types.h>\nstruct device;\nstruct nvs_fs { const struct device *flash_device; off_t offset; uint32_t sector_size; uint32_t sector_count; };\nextern \"C\" {\nint nvs_mount(struct nvs_fs *fs);\nssize_t nvs_read(struct nvs_fs *fs, uint16_t id, void *data, size_t len);\nssize_t nvs_write(struct nvs_fs *fs, uint16_t id, const void *data, size_t len);\n}\n")
file(MAKE_DIRECTORY ${MOCK_DIR}/zephyr/sys)
file(WRITE ${MOCK_DIR}/zephyr/sys/reboot.h "#pragma once\n#define SYS_REBOOT_COLD 0\n#define SYS_REBOOT_WARM 1\nextern \"C\" void sys_reboot(int type);\n")
file(MAKE_DIRECTORY ${MOCK_DIR}/zephyr/storage)
file(WRITE ${MOCK_DIR}/zephyr/storage/flash_map.h "#pragma once\n#include <stdint.h>\n#include <sys/types.h>\n#define FIXED_PARTITION_ID(...) 0\n#define FIXED_PARTITION_DEVICE(...) nullptr\n#define FIXED_PARTITION_OFFSET(...) 0\nstruct flash_area { uint8_t fa_id; };\nextern \"C\" {\nint flash_area_open(uint8_t id, const struct flash_area **fa);\nvoid flash_area_close(const struct flash_area *fa);\nint flash_area_read(const struct flash_area *fa, off_t off, void *dst, size_t len);\nint flash_area_write(const struct flash_area *fa, off_t off, const void *src, size_t len);\n}\n")

# 3. Generate a mock globals file for external hardware pointers and virtual time
# 3. Generate a mock globals file for external hardware pointers and virtual time
file(WRITE ${CMAKE_BINARY_DIR}/mock_globals.cpp "#include <zephyr/device.h>\n#include <stdint.h>\n#include <zephyr/pm/pm.h>\n#include <zephyr/pm/device.h>\n#include <zephyr/drivers/watchdog.h>\n#include <zephyr/drivers/counter.h>\n\n__attribute__((weak)) const device* i2c_hardware = nullptr;\n\nuint32_t virtual_uptime = 0;\n\nextern \"C\" {\n__attribute__((weak)) bool device_is_ready(const struct device *dev) { return true; }\n__attribute__((weak)) int wdt_install_timeout(const struct device *dev, const struct wdt_timeout_cfg *cfg) { return 0; }\n__attribute__((weak)) int wdt_setup(const struct device *dev, uint8_t options) { return 0; }\n__attribute__((weak)) int wdt_feed(const struct device *dev, int channel_id) { return 0; }\n__attribute__((weak)) void pm_policy_state_lock_get(uint8_t state, uint8_t substate_id) {}\n__attribute__((weak)) void pm_policy_state_lock_put(uint8_t state, uint8_t substate_id) {}\n__attribute__((weak)) int pm_device_action_run(const struct device *dev, enum pm_device_action action) { return 0; }\n__attribute__((weak)) uint32_t counter_us_to_ticks(const struct device *dev, uint64_t us) { return 0; }\n__attribute__((weak)) int counter_set_channel_alarm(const struct device *dev, uint8_t chan_id, const struct counter_alarm_cfg *alarm_cfg) { return 0; }\n}\n")

# 4. Define the executable
add_executable(run_pm_tests 
    test_power_management.cpp 
    ../src/Power_Management_System.cpp
    ${CMAKE_BINARY_DIR}/mock_globals.cpp
)

add_executable(run_usb_shell_tests 
    test_usb_shell.cpp 
    ../src/USB_CDC_Virtual_COM_Shell_Interface.cpp
    ../src/Device_State_Machine+Watchdog.cpp
    ../src/Fault_Tolerant_I2C_Communication_Layer.cpp
    ../src/Smart_Battery_System.cpp
    ../src/Persistent_Configuration_System.cpp
    ${CMAKE_BINARY_DIR}/mock_globals.cpp
)

add_executable(run_battery_tests
    test_battery_fsm.cpp
    ../src/Smart_Battery_System.cpp
    ../src/Fault_Tolerant_I2C_Communication_Layer.cpp
    ../src/Device_State_Machine+Watchdog.cpp
    ${CMAKE_BINARY_DIR}/mock_globals.cpp
)

add_executable(run_static_memory_tests
    test_static_memory.cpp
    ../src/Static_Memory+MISRA_Compliance_Layer.cpp
    ${CMAKE_BINARY_DIR}/mock_globals.cpp
)

add_executable(run_config_tests
     test_config_store.cpp
     ../src/Persistent_Configuration_System.cpp
     ${CMAKE_BINARY_DIR}/mock_globals.cpp
)

add_executable(run_i2c_faults_tests
      test_I2C_Fault_Recovery.cpp
      ../src/Fault_Tolerant_I2C_Communication_Layer.cpp
      ${CMAKE_BINARY_DIR}/mock_globals.cpp
)

add_executable(run_state_machine_tests
       test_state_machine.cpp
       ../src/Device_State_Machine+Watchdog.cpp
       ${CMAKE_BINARY_DIR}/mock_globals.cpp
)

add_executable(run_sync_tests
	test_synchronization.cpp
	../src/RTOS_Synchronization_Layer.cpp
	${CMAKE_BINARY_DIR}/mock_globals.cpp
)

add_executable(run_command_tests
	test_rtos_commands.cpp
	../src/RTOS_Command_based_thread_system.cpp
        ../src/Device_State_Machine+Watchdog.cpp
        ../src/Power_Management_System.cpp
	${CMAKE_BINARY_DIR}/mock_globals.cpp
)

# 5. Include our auto-generated mocks and your real headers
target_include_directories(run_pm_tests PRIVATE 
    ${MOCK_DIR}
    ../include
)

target_include_directories(run_usb_shell_tests PRIVATE 
    ${MOCK_DIR}
    ../include
)

target_include_directories(run_battery_tests PRIVATE
    ${MOCK_DIR}
    ../include
)

target_include_directories(run_static_memory_tests PRIVATE
   ${MOCK_DIR}
   ../include
)

target_include_directories(run_config_tests PRIVATE
   ${MOCK_DIR}		
   ../include
)

target_include_directories(run_i2c_faults_tests PRIVATE
   ${MOCK_DIR}
   ../include
)

target_include_directories(run_state_machine_tests PRIVATE
   ${MOCK_DIR}
   ../include
)

target_include_directories(run_sync_tests PRIVATE
   ${MOCK_DIR}
   ../include
)

target_include_directories(run_command_tests PRIVATE
   ${MOCK_DIR}
   ../include
)

# 6. Link GTest and threading libraries
target_link_libraries(run_pm_tests PRIVATE GTest::gtest_main pthread)
target_link_libraries(run_usb_shell_tests PRIVATE GTest::gtest_main pthread)
target_link_libraries(run_battery_tests PRIVATE GTest::gtest_main pthread)
target_link_libraries(run_static_memory_tests PRIVATE GTest::gtest_main pthread)
target_link_libraries(run_config_tests PRIVATE GTest::gtest_main pthread)
target_link_libraries(run_i2c_faults_tests PRIVATE GTest::gtest_main pthread)
target_link_libraries(run_state_machine_tests PRIVATE GTest::gtest_main pthread)
target_link_libraries(run_sync_tests PRIVATE GTest::gtest_main pthread)
target_link_libraries(run_command_tests PRIVATE GTest::gtest_main pthread)

# Enable GCOV profiling flags for compilation and linking
target_compile_options(run_pm_tests PRIVATE --coverage)
target_link_options(run_pm_tests PRIVATE --coverage)

target_compile_options(run_usb_shell_tests PRIVATE --coverage)
target_link_options(run_usb_shell_tests PRIVATE --coverage)

target_compile_options(run_battery_tests PRIVATE --coverage)
target_link_options(run_battery_tests PRIVATE --coverage)

target_compile_options(run_static_memory_tests PRIVATE --coverage)
target_link_options(run_static_memory_tests PRIVATE --coverage)

target_compile_options(run_config_tests PRIVATE --coverage)
target_link_options(run_config_tests PRIVATE --coverage)

target_compile_options(run_i2c_faults_tests PRIVATE --coverage)
target_link_options(run_i2c_faults_tests PRIVATE --coverage)

target_compile_options(run_state_machine_tests PRIVATE --coverage)
target_link_options(run_state_machine_tests PRIVATE --coverage)

target_compile_options(run_sync_tests PRIVATE --coverage)
target_link_options(run_sync_tests PRIVATE --coverage)

target_compile_options(run_command_tests PRIVATE --coverage)
target_link_options(run_command_tests PRIVATE --coverage)

target_compile_definitions(run_static_memory_tests PRIVATE IS_TEST_ENVIRONMENT)
target_compile_definitions(run_pm_tests PRIVATE IS_TEST_ENVIRONMENT)
target_compile_definitions(run_usb_shell_tests PRIVATE IS_TEST_ENVIRONMENT)
target_compile_definitions(run_battery_tests PRIVATE IS_TEST_ENVIRONMENT)
target_compile_definitions(run_config_tests PRIVATE IS_TEST_ENVIRONMENT)
target_compile_definitions(run_sync_tests PRIVATE IS_TEST_ENVIRONMENT)
target_compile_definitions(run_command_tests PRIVATE IS_TEST_ENVIRONMENT)
