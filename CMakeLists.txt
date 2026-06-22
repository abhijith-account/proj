cmake_minimum_required(VERSION 3.20.0)

# Pull in the Zephyr build system
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(medical_firmware)

zephyr_cc_option(-Wall -Wextra -Werror)
zephyr_cxx_option(-Wall -Wextra -Werror -ped)

if(CONFIG_COVERAGE)
	zephyr_cc_option(--coverage -O0)
	zephyr_cxx_option(--coverage)
endif()	

# Link your C++ files here
target_sources(app PRIVATE src/main.cpp)
