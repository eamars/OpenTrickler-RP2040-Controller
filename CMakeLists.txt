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

# OTA flash split: physical flash is divided into three regions -- a small
# bootloader (always at offset 0), the app (chain-loaded by the bootloader),
# and an OTA staging area (the upper half of the chip, unchanged in size from
# earlier revisions of this split). See bootloader/main.c and
# src/firmware_update.c for how these are used. The linker regions for both
# the "bootloader" and "app" targets are set via a POST-pico_sdk_init()
# override of their generated linker scripts, near the bottom of this file
# and in bootloader/CMakeLists.txt, not via a PICO_FLASH_SIZE_BYTES variable
# set here. See the comment at that override for why: pre-setting
# PICO_FLASH_SIZE_BYTES before pico_sdk_init() was tried first and caused a
# severe, silent bug -- root-caused and fixed; kept here as a warning so it
# isn't reintroduced.

# Include the Pico SDK
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

# Name the project
project(${PROJECT_NAME}
        LANGUAGES C CXX ASM
        DESCRIPTION "Firmware for OpenTrickler Controller"
)

# Initialise the Pico SDK
pico_sdk_init()

# ---- OTA flash layout (shared by the "app" and "bootloader" targets) ----
# Physical chip is split in half: the lower half holds [bootloader | app],
# the upper half is the OTA staging area (unchanged from earlier revisions
# of this split -- only how the lower half subdivides is new). Computed once
# here, before either add_subdirectory() below, so both targets' compile
# definitions and linker regions derive from the same numbers.
set(BOOTLOADER_FLASH_SIZE_BYTES 65536)   # 64 KiB -- ample for a footer-check + flash-copy + chain-load
if(PICO_BOARD STREQUAL "pico2_w")
    set(FIRMWARE_UPDATE_TOTAL_FLASH_SIZE 4194304)   # 4 MiB physical chip
    set(FIRMWARE_UPDATE_BOARD_TYPE FIRMWARE_BOARD_TYPE_PICO2_W)
    set(FIRMWARE_UPDATE_BATCH_SIZE 131072)          # 128 KiB (32 sectors) -- upload flash-write batch size
else()
    set(FIRMWARE_UPDATE_TOTAL_FLASH_SIZE 2097152)   # 2 MiB physical chip
    set(FIRMWARE_UPDATE_BOARD_TYPE FIRMWARE_BOARD_TYPE_PICO_W)
    set(FIRMWARE_UPDATE_BATCH_SIZE 32768)           # 32 KiB (8 sectors) -- upload flash-write batch size
endif()
math(EXPR FIRMWARE_UPDATE_STAGING_OFFSET "${FIRMWARE_UPDATE_TOTAL_FLASH_SIZE} / 2")
set(FIRMWARE_UPDATE_STAGING_SIZE ${FIRMWARE_UPDATE_STAGING_OFFSET})
set(FIRMWARE_UPDATE_APP_REGION_OFFSET ${BOOTLOADER_FLASH_SIZE_BYTES})
math(EXPR FIRMWARE_UPDATE_APP_REGION_SIZE "${FIRMWARE_UPDATE_STAGING_OFFSET} - ${BOOTLOADER_FLASH_SIZE_BYTES}")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Set flags and directory variables
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")
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

# Pull in the OTA second-stage bootloader (separate executable, see
# bootloader/CMakeLists.txt) -- it consumes the OTA flash layout variables
# set above.
add_subdirectory(bootloader)

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
    hardware_flash
    pico_flash
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
# set( CMAKE_VERBOSE_MAKEFILE on )

# Generate extra outputs
pico_add_extra_outputs("${TARGET_NAME}")

# OTA firmware image: append the footer (see src/firmware_footer.h) to the
# plain app.bin that pico_add_extra_outputs just produced, yielding
# app_ota.bin -- the file users upload through the web UI. This must be
# registered AFTER pico_add_extra_outputs (which is what actually adds the
# POST_BUILD step that generates app.bin from app.elf), since POST_BUILD
# commands on a target run in the order they were added.
#
# find_package is re-run here (rather than reusing src/CMakeLists.txt's call)
# because Python_EXECUTABLE is scoped to the directory that found it and does
# not propagate back up through add_subdirectory(); this call is cheap since
# CMake already cached the interpreter location.
find_package(Python COMPONENTS Interpreter REQUIRED)
add_custom_command(TARGET "${TARGET_NAME}" POST_BUILD
    COMMAND "${Python_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/append_firmware_footer.py"
            --input "$<TARGET_FILE_DIR:${TARGET_NAME}>/${TARGET_NAME}.bin"
            --output "$<TARGET_FILE_DIR:${TARGET_NAME}>/${TARGET_NAME}_ota.bin"
            --board "${PICO_BOARD}"
    COMMENT "Appending OTA footer -> ${TARGET_NAME}_ota.bin"
    VERBATIM
)

# OTA flash split: shrink the linker's FLASH region for the "app" target to
# FIRMWARE_UPDATE_APP_REGION_SIZE, starting at FIRMWARE_UPDATE_APP_REGION_OFFSET
# (i.e. right after the bootloader's own 64 KiB region -- see
# bootloader/CMakeLists.txt for its region, and bootloader/main.c for how it
# chain-loads into this one). The upper half of the chip remains the OTA
# staging area (see src/firmware_update.c).
#
# This directly overwrites the tiny pico_flash_region.ld fragment that
# pico_standard_link's own CMakeLists.txt already generated (using the
# board's full physical flash size) during pico_sdk_init() above. Placed
# here, after everything else in this file, this file(WRITE) is guaranteed
# to be the last write to that path during this CMake configure run,
# regardless of exactly when the SDK generated its own version internally --
# no need to know or depend on that internal ordering.
#
# WHY NOT set(PICO_FLASH_SIZE_BYTES ...) BEFORE pico_sdk_init(), as tried
# originally: pico_w.h/pico2_w.h declare their physical flash size via a
# magic "pico_cmake_set_default PICO_FLASH_SIZE_BYTES = ..." comment pragma
# that the SDK's generic_board.cmake scans for. If that exact CMake variable
# name is *already defined* by the time pico_sdk_init() parses that pragma,
# the SDK assumes the user is deliberately overriding a board default and
# promotes it to a GLOBAL C COMPILE DEFINITION via pico_base_headers (see
# library/pico-sdk/cmake/generic_board.cmake and
# library/pico-sdk/src/common/pico_base_headers/CMakeLists.txt) -- silently
# defeating the whole point, since hardware_flash's flash_range_erase/
# program bounds-check against that same macro name, expecting the
# *physical* chip size:
#   hard_assert(flash_offs + count <= PICO_FLASH_SIZE_BYTES);   // flash.c
# With the shrunk (app-region) size compiled in instead, every OTA write
# into the staging region (which starts exactly at that shrunk value) failed
# this assert -- a silent, total, unrecoverable hang on every single OTA
# flash write, independent of any multicore-safety configuration, since the
# assert fires inside flash_range_erase/program itself, before any
# multicore coordination is even reached. Confirmed directly by inspecting
# the generated build.ninja: hardware_flash/flash.c.obj was being compiled
# with -DPICO_FLASH_SIZE_BYTES="<shrunk value>". Never pre-set
# PICO_FLASH_SIZE_BYTES (or any other name a board header declares via a
# pico_cmake_set/pico_cmake_set_default pragma) before pico_sdk_init() for
# this reason -- override the generated linker fragment afterward instead,
# as done here.
math(EXPR OTA_APP_FLASH_ORIGIN "0x10000000 + ${FIRMWARE_UPDATE_APP_REGION_OFFSET}" OUTPUT_FORMAT HEXADECIMAL)
file(WRITE "${CMAKE_BINARY_DIR}/pico_flash_region.ld"
     "FLASH(rx) : ORIGIN = ${OTA_APP_FLASH_ORIGIN}, LENGTH = ${FIRMWARE_UPDATE_APP_REGION_SIZE}\n")
