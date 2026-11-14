cmake_minimum_required(VERSION 3.25)

# Set project data
set(PROJECT_NAME "OpenTricklerController")
set(TARGET_NAME "app")

message("CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")

# Set env variable 'PICO_SDK_PATH' to the local Pico SDK
# Comment out the set() if you have a global copy of the
# SDK set and $PICO_SDK_PATH defined in your $PATH
set(ENV{PICO_SDK_PATH} "${CMAKE_SOURCE_DIR}/library/pico-sdk")
set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})

# Include the Pico SDK
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

# Name the project
project(${PROJECT_NAME}
        LANGUAGES C CXX ASM
        DESCRIPTION "Firmware for OpenTrickler Controller"
)

# Initialise the Pico SDK
pico_sdk_init()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Set flags and directory variables
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -DDEBUG")
set(SRC_DIRECTORY "${CMAKE_SOURCE_DIR}/src")
set(FREERTOS_SRC_DIRECTORY "${CMAKE_SOURCE_DIR}/library/FreeRTOS-Kernel")
set(U8G2_SRC_DIRECYTORY "${CMAKE_SOURCE_DIR}/library/u8g2")
set(U8G2_MUI_DIRECTORY "${U8G2_SRC_DIRECYTORY}/csrc")
set(TMC_SRC_DIRECTORY "${CMAKE_SOURCE_DIR}/library/Trinamic-library")
set(SCRIPTS_DIRECTORY "${CMAKE_SOURCE_DIR}/scripts")
set(PICO_BOARD_HEADER_DIRS "${CMAKE_SOURCE_DIR}/targets")

# Append compiler flags
SET(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -fdata-sections -ffunction-sections -Wall")
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -fdata-sections -ffunction-sections -Wall")
# SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")


# Application
add_executable("${TARGET_NAME}")

# Include source
include_directories(${SRC_DIRECTORY})

# Pull in FreeRTOS
include(${FREERTOS_SRC_DIRECTORY}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

# Pull in u8g2
add_subdirectory(${U8G2_SRC_DIRECYTORY})

# Pull in Trinamic library (we don't use CMakeLists provided by TMC library as I don't want hal stuff)
add_library(trinamic INTERFACE)
target_sources(trinamic INTERFACE
    ${TMC_SRC_DIRECTORY}/common.c
    ${TMC_SRC_DIRECTORY}/tmc_interface.c
    ${TMC_SRC_DIRECTORY}/tmc2130.c
    ${TMC_SRC_DIRECTORY}/tmc2209.c
    ${TMC_SRC_DIRECTORY}/tmc5160.c
)
target_include_directories(trinamic INTERFACE ${TMC_SRC_DIRECTORY})



# Pull in u8g2 mui
add_library(u8g2_mui
    ${U8G2_MUI_DIRECTORY}/mui_u8g2.c
    ${U8G2_MUI_DIRECTORY}/mui.c
    ${U8G2_MUI_DIRECTORY}/u8g2_button.c
    ${U8G2_MUI_DIRECTORY}/u8g2_box.c
)
target_link_libraries(u8g2_mui u8g2)

# Pull in src
add_subdirectory(${SRC_DIRECTORY})

# Collect all source files
file(GLOB SRC ${SRC_DIRECTORY}/*.c 
              ${SRC_DIRECTORY}/*.cpp)

# Include application source file
target_sources("${TARGET_NAME}" PUBLIC
    ${SRC}
)

# Include additional headers
target_include_directories("${TARGET_NAME}" PUBLIC ${CMAKE_SOURCE_DIR}/targets)

# Include libraries1
target_link_libraries("${TARGET_NAME}"
    pico_stdlib
    hardware_pio
    hardware_spi
    hardware_i2c
    hardware_pwm
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap4
    u8g2
    u8g2_mui
    trinamic
    app_version

    # Required by cyw43 wireless module
    pico_cyw43_arch_lwip_sys_freertos
    pico_lwip_freertos
    pico_lwip_mdns
    # pico_lwip_http
)

target_link_options("${TARGET_NAME}" PUBLIC -Wl,--gc-sections -Wl,--print-memory-usage)

# The console (ADR-001): stdio over RTT, read through a debug probe. Never on UART0, the scale's port
pico_enable_stdio_rtt("${TARGET_NAME}" 1)
pico_enable_stdio_uart("${TARGET_NAME}" 0)
pico_enable_stdio_usb("${TARGET_NAME}" 0)
target_compile_definitions("${TARGET_NAME}" PRIVATE
    # A failed assert() enters the USB loader in place of a breakpoint; that resets PIO, so the steps stop
    PICO_ENTER_USB_BOOT_ON_EXIT=1
    # RTT buffers: out large enough for a snap's lines, in for a whole command line (SEGGER's are 1 KB and 16 B)
    BUFFER_SIZE_UP=2048
    BUFFER_SIZE_DOWN=256
)

# A test build that faults ten seconds after start-up: -DRC_TEST_FAULT=1 asserts, 2 makes a hard fault
set(RC_TEST_FAULT "" CACHE STRING "Test build only: 1 asserts, 2 hard-faults ten seconds after start-up")
if (RC_TEST_FAULT)
    target_compile_definitions("${TARGET_NAME}" PRIVATE RC_TEST_FAULT=${RC_TEST_FAULT})
endif()
# set( CMAKE_VERBOSE_MAKEFILE on )

# Generate extra outputs
pico_add_extra_outputs("${TARGET_NAME}")
