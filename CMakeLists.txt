cmake_minimum_required(VERSION 3.14)


# Set project data
set(PROJECT_NAME "OpenTricklerController")

message("CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")

# Set env variable 'PICO_SDK_PATH' to the local Pico SDK
# Comment out the set() if you have a global copy of the
# SDK set and $PICO_SDK_PATH defined in your $PATH
set(ENV{PICO_SDK_PATH} "${CMAKE_SOURCE_DIR}/library/pico-sdk")
set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})

# Set flags and directory variables
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -DDEBUG")
set(SRC_DIRECTORY "${CMAKE_SOURCE_DIR}/src")
set(FREERTOS_SRC_DIRECTORY "${CMAKE_SOURCE_DIR}/library/FreeRTOS-Kernel")
set(U8G2_SRC_DIRECYTORY "${CMAKE_SOURCE_DIR}/library/u8g2")
set(U8G2_MUI_DIRECTORY "${U8G2_SRC_DIRECYTORY}/csrc")
set(TMC_SRC_DIRECTORY "${CMAKE_SOURCE_DIR}/library/Trinamic-library")
set(PICO_BOARD_HEADER_DIRS "${CMAKE_SOURCE_DIR}/targets")

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
    hardware_pio
    hardware_spi
    pico_cyw43_arch_none
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap4
    u8g2
    u8g2_mui
    trinamic
)

# if (BOARD_TYPE STREQUAL "pico_w" )
#     message("Using PICO_W board, link cyw43")
#     target_link_libraries(app pico_cyw43_arch_none)
# endif()

target_link_options(app PRIVATE -Wl,--gc-sections)
# set( CMAKE_VERBOSE_MAKEFILE on )

# Generate extra input
pico_add_extra_outputs(app)
