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


# Initialise the Pico SDK
pico_sdk_init()

# Pull in FreeRTOS
include(${FREERTOS_SRC_DIRECTORY}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

# Include source
include_directories(${SRC_DIRECTORY})

# Include application source file
add_executable(app
    ${SRC_DIRECTORY}/app.c
)

# Include libraries1
target_link_libraries(app
    pico_stdlib
    FreeRTOS-Kernel FreeRTOS-Kernel-Heap1
)