cmake_minimum_required(VERSION 3.14)


# Set project data
set(PROJECT_NAME "OpenTricklerController")

message("CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")

# Set env variable 'PICO_SDK_PATH' to the local Pico SDK
# Comment out the set() if you have a global copy of the
# SDK set and $PICO_SDK_PATH defined in your $PATH
set(ENV{PICO_SDK_PATH} "${CMAKE_SOURCE_DIR}/target/pico-sdk")
set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})

# Set flags and directory variables
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -DDEBUG")
set(SRC_DIRECTORY "${CMAKE_SOURCE_DIR}/src")
set(FREERTOS_SRC_DIRECTORY "${CMAKE_SOURCE_DIR}/rtos/FreeRTOS-Kernel")

# Include the Pico SDK
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

# Name the project
project(${PROJECT_NAME}
        LANGUAGES C CXX ASM
        DESCRIPTION "Firmware for OpenTrickler Controller"
)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Append compiler flags
SET(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -fdata-sections -ffunction-sections")
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -fdata-sections -ffunction-sections")
# SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")


# Initialise the Pico SDK
pico_sdk_init()

# Application
add_executable(app)

# Include source
include_directories(${SRC_DIRECTORY})

# Pull in FreeRTOS
include(${FREERTOS_SRC_DIRECTORY}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

# Pull in u8g2
add_subdirectory(${CMAKE_SOURCE_DIR}/graphics/u8g2)

# Pull in u8g2 mui
set(U8G2_MUI_DIRECTORY "${CMAKE_SOURCE_DIR}/graphics/u8g2/csrc")
add_library(u8g2_mui
    ${U8G2_MUI_DIRECTORY}/mui_u8g2.c
    ${U8G2_MUI_DIRECTORY}/mui.c
    ${U8G2_MUI_DIRECTORY}/u8g2_button.c
    ${U8G2_MUI_DIRECTORY}/u8g2_box.c
)

# Pull in src
add_subdirectory(${CMAKE_SOURCE_DIR}/src)

# Collect all source files
file(GLOB SRC ${SRC_DIRECTORY}/*.c ${SRC_DIRECTORY}/*.cpp)

# Include application source file
target_sources(app PRIVATE
    ${SRC}
)

# Include libraries1
target_link_libraries(app
    pico_stdlib
    hardware_spi
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap4
    pico_cyw43_arch_none
    u8g2
    u8g2_mui
)

target_link_options(app PRIVATE -Wl,--gc-sections)
# set( CMAKE_VERBOSE_MAKEFILE on )

