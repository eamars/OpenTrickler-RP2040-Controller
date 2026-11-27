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

# The console (ADR-001): stdio over RTT, read through a debug probe. Never on UART0, the scale's port. Chosen here
# for every target, not on the image alone (pico_enable_stdio_rtt() and the like): the SDK's libraries give a target
# the headers of the console it chose, and a module is a target of its own (ADR-006)
set(PICO_STDIO_RTT 1)
set(PICO_STDIO_UART 0)
set(PICO_STDIO_USB 0)

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
set(LVGL_DIRECTORY "${CMAKE_SOURCE_DIR}/library/lvgl")
set(SCRIPTS_DIRECTORY "${CMAKE_SOURCE_DIR}/scripts")
set(PICO_BOARD_HEADER_DIRS "${CMAKE_SOURCE_DIR}/src/boards/common")

# Append compiler flags
SET(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -fdata-sections -ffunction-sections -Wall")
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -fdata-sections -ffunction-sections -Wall")
# SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")


# Python makes the version, the pages and the fonts
find_package(Python COMPONENTS Interpreter REQUIRED)

# The image
add_executable("${TARGET_NAME}")

# Our own code is modules (ADR-006): src/<layer>/<module>/, each built on its own by its CMakeLists.txt, which names
# what it needs. The board says which modules its build carries, links the third-party code into the image and
# gives it to the modules by name
include(${CMAKE_SOURCE_DIR}/cmake/ot_module.cmake)
if (NOT EXISTS ${SRC_DIRECTORY}/boards/${PICO_BOARD}/board.cmake)
    message(FATAL_ERROR "No board ${PICO_BOARD}: give -DPICO_BOARD=pico_w or pico2_w (src/boards/<board>/board.cmake)")
endif()
include(${SRC_DIRECTORY}/boards/${PICO_BOARD}/board.cmake)
# What a build carries (ADR-008, gate 2; ADR-005's feature table): a part is in the build or out of it, never built a
# second way, and the unit reports each by name (the index's `features`), made from the board's list (ADR-006, gate 9)
ot_board_resolve("${TARGET_NAME}")
ot_image_settings("${TARGET_NAME}")
foreach(module IN LISTS OT_BOARD_MODULES)
    add_subdirectory(${SRC_DIRECTORY}/${module})
endforeach()
# One chip's side of the platform and one view in a build, none missing (ADR-006, gate 9: "one of")
ot_check_places(platform view)
ot_link_modules("${TARGET_NAME}")

target_link_options("${TARGET_NAME}" PUBLIC -Wl,--gc-sections -Wl,--print-memory-usage)

# The console (ADR-001) is chosen above, before the SDK starts: stdio over RTT
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
# A test build whose start skips its Wi-Fi join, as if the router were down (ADR-004): it joins again later
option(RC_TEST_WIFI_FIRST_JOIN_FAILS "Test build only: the start's Wi-Fi join is skipped" OFF)
if (RC_TEST_WIFI_FIRST_JOIN_FAILS)
    target_compile_definitions("${TARGET_NAME}" PRIVATE RC_TEST_WIFI_FIRST_JOIN_FAILS=1)
endif()
# A test build whose radio does not start (ADR-006, gate 10): the unit goes on without the network
option(RC_TEST_RADIO_FAILS "Test build only: the radio's start skipped, as if it had failed" OFF)
if (RC_TEST_RADIO_FAILS)
    target_compile_definitions("${TARGET_NAME}" PRIVATE RC_TEST_RADIO_FAILS=1)
endif()
# A probe build (ADR-008, gate 1): the start's join skipped, so the unit starts as its own access point; the station
# joins only on the console's `wifi join`, and the access point is kept up when it does; `wifi scan` lists networks
option(RC_TEST_AP_PROBE "Test build only: the access point kept up through the station's join, wifi join and scan" OFF)
if (RC_TEST_AP_PROBE)
    target_compile_definitions("${TARGET_NAME}" PRIVATE RC_TEST_AP_PROBE=1)
endif()
# A test build whose stored LED record has another size (ADR-008, gate 2): on trial its settings read as a layout that
# changed, held as the defaults; kept, the update says it reset them
option(RC_TEST_LAYOUT_CHANGE "Test build only: the LEDs' stored record a word longer" OFF)
if (RC_TEST_LAYOUT_CHANGE)
    target_compile_definitions("${TARGET_NAME}" PRIVATE RC_TEST_LAYOUT_CHANGE=1)
endif()
# A test build that never confirms itself on trial (ADR-004): the unit goes back to the other slot
option(RC_TEST_NO_CONFIRM "Test build only: an image on trial never confirms itself" OFF)
if (RC_TEST_NO_CONFIRM)
    target_compile_definitions("${TARGET_NAME}" PRIVATE RC_TEST_NO_CONFIRM=1)
endif()
# A test build that hangs at its first line (ADR-004, review Q5): only the boot's own watchdog, the RP2040 bootloader's
# or the RP2350 ROM's, ends its trial
option(RC_TEST_HANG_AT_START "Test build only: the image hangs before it starts anything" OFF)
if (RC_TEST_HANG_AT_START)
    target_compile_definitions("${TARGET_NAME}" PRIVATE RC_TEST_HANG_AT_START=1)
endif()
# A test build that carries no A&D FX-i make (ADR-006, gate 9): a unit whose stored make is the A&D's starts with no
# scale read in its place, and refuses a charge and a calibration
option(RC_TEST_NO_AND_FXI "Test build only: the A&D FX-i's make left out" OFF)
if (RC_TEST_NO_AND_FXI)
    target_compile_definitions("${TARGET_NAME}" PRIVATE RC_TEST_NO_AND_FXI=1)
endif()
# The settings' storage when it fails (ADR-017, gate 1), one test build each: the EEPROM looked for where no chip
# answers; its bus's data line held low; the charge's record read with a byte wrong; a save of the powders cut after
# three pages of its place by a restart; a stored make of scale that no make has, written at the start
foreach(test_build EEPROM_ABSENT I2C_HELD_LOW RECORD_DAMAGED SAVE_CUT SCALE_MAKE_UNKNOWN)
    option(RC_TEST_${test_build} "Test build only (ADR-017, gate 1)" OFF)
    if (RC_TEST_${test_build})
        target_compile_definitions("${TARGET_NAME}" PRIVATE RC_TEST_${test_build}=1)
    endif()
endforeach()
# set( CMAKE_VERBOSE_MAKEFILE on )

# The flash layout and the image's metadata block (ADR-004)
include(${SRC_DIRECTORY}/boards/common/flash_layout.cmake)

# Generate extra outputs
pico_add_extra_outputs("${TARGET_NAME}")
