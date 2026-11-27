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


# Application
add_executable("${TARGET_NAME}")

# Our own code (ADR-006, gate 2): src/<layer>/<module>/, a module's headers for others in its include/. Until
# each module is built on its own (gate 3) every module's include/ is on the one include path, as all of src/ was;
# a header a module keeps to itself lies beside its sources and is found from there
file(GLOB OT_INCLUDE_DIRS LIST_DIRECTORIES true ${SRC_DIRECTORY}/*/*/include)
list(APPEND OT_INCLUDE_DIRS ${SRC_DIRECTORY}/platform/include ${SRC_DIRECTORY}/boards/common)
include_directories(${OT_INCLUDE_DIRS})

# Pull in FreeRTOS. Its location is given, not searched for: upstream's import file (V11.3.3), left to search,
# finds the kernel as the SDK's sibling on its first pass and stops there with that pass's path to the RP2350 port,
# which only Raspberry Pi's fork had. Given the location, it tries both paths and takes the RP2350 port from the
# community ports (ADR-011).
set(FREERTOS_KERNEL_PATH ${FREERTOS_SRC_DIRECTORY})
include(${FREERTOS_SRC_DIRECTORY}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

# LVGL, configured by the view's lv_conf.h (src/ui/view_12864/include). Every source is compiled; what lv_conf.h switches off compiles to nothing.
# At -Os in every build, as the owner decided after ADR-010's gate 1: code nobody steps through, and at -O0 the
# Pico W's image did not fit its slot
file(GLOB_RECURSE LVGL_SOURCES ${LVGL_DIRECTORY}/src/*.c)
add_library(lvgl STATIC ${LVGL_SOURCES})
target_include_directories(lvgl PUBLIC ${LVGL_DIRECTORY}/include ${LVGL_DIRECTORY}
                           ${SRC_DIRECTORY}/ui/view_12864/include)
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)
target_compile_options(lvgl PRIVATE -Os)

# The firmware's modules (ADR-006): ot_module(), and the image linking each whole
include(${CMAKE_SOURCE_DIR}/cmake/ot_module.cmake)

# Pull in src
add_subdirectory(${SRC_DIRECTORY})

# Collect all source files
file(GLOB SRC ${SRC_DIRECTORY}/*/*/*.c
              ${SRC_DIRECTORY}/*/*/*.cpp)
# But those a module builds (ADR-006, gate 1: two modules tried, the rest one program until gate 3)
get_property(OT_MODULE_SOURCES GLOBAL PROPERTY OT_MODULE_SOURCES)
list(REMOVE_ITEM SRC ${OT_MODULE_SOURCES})

# What a build carries (ADR-008, gate 2; ADR-005's feature table): a feature is in the build or out of it, never built
# a second way, and the unit reports each by name (the index's `features`). On by default on both boards
option(FEATURE_SERVO_GATE "The servo gate: its module, its settings, its menu page and its API" ON)
if (NOT FEATURE_SERVO_GATE)
    list(REMOVE_ITEM SRC ${SRC_DIRECTORY}/services/servo_gate/servo_gate.c)
endif()
message(STATUS "Features: servo_gate=${FEATURE_SERVO_GATE}")

# Include application source file
target_sources("${TARGET_NAME}" PUBLIC
    ${SRC}
)

# Include additional headers
if (FEATURE_SERVO_GATE)
    target_compile_definitions("${TARGET_NAME}" PRIVATE FEATURE_SERVO_GATE=1)
else()
    target_compile_definitions("${TARGET_NAME}" PRIVATE FEATURE_SERVO_GATE=0)
endif()

# Include libraries1
target_link_libraries("${TARGET_NAME}"
    pico_stdlib
    hardware_pio
    hardware_spi
    hardware_i2c
    hardware_pwm
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap4
    lvgl
    app_version

    # Required by cyw43 wireless module
    pico_cyw43_arch_lwip_sys_freertos
    pico_lwip_freertos
    pico_lwip_mdns

    # flash_safe_execute(): flash written with the other core held off it, as the ROM's confirmation of an image
    # does (ADR-004)
    pico_flash
    # mbedTLS, with the boards' mbedtls_config.h: the SHA-256 an image is checked by before it is tried (ADR-004), and the
    # TLS client the update's poll and download fetch by (ADR-010, gate 5; https_client.c)
    pico_mbedtls
)

# Third-party code as a module sees it (ADR-006): its include paths and no source, under ESP-IDF's names for what
# both platforms have, so a module's REQUIRES line is the same under ESP-IDF. The real libraries, which carry their
# sources, are linked once, above.
#   - rp2_sdk is the Pico SDK itself. Every module that requires it is one gate 7 has still to free of the chip.
#   - lwip carries the radio's driver too (cyw43), until gate 7 puts it behind the network's interface.
ot_third_party(rp2_sdk pico_stdlib hardware_pio hardware_spi hardware_i2c hardware_pwm pico_flash)
ot_third_party(freertos FreeRTOS-Kernel FreeRTOS-Kernel-Heap4)
ot_third_party(lwip pico_cyw43_arch_lwip_sys_freertos pico_lwip_freertos pico_lwip_mdns)
ot_third_party(mbedtls pico_mbedtls)
ot_third_party(lvgl lvgl)
ot_image_settings("${TARGET_NAME}")
ot_link_modules("${TARGET_NAME}")

# SHA-256 optimised: at the build's -O0 an image's check took 5 s on the Pico W, and the web server, whose task it
# runs in, answered nothing meanwhile (ADR-004). It is third-party code nobody steps through
set_source_files_properties("${PICO_SDK_PATH}/lib/mbedtls/library/sha256.c" PROPERTIES COMPILE_OPTIONS "-O2")
# The rest of mbedTLS at -Os in every build, as the owner decided after ADR-010's gate 1: at -O0 a handshake took
# 148 s on the Pico 2 W, and its assembly multiply does not compile
file(GLOB MBEDTLS_SOURCES ${PICO_SDK_PATH}/lib/mbedtls/library/*.c)
list(FILTER MBEDTLS_SOURCES EXCLUDE REGEX "/sha256\\.c$")
set_source_files_properties(${MBEDTLS_SOURCES} PROPERTIES COMPILE_OPTIONS "-Os")
# The rest of the third-party code at -Os in every build but FreeRTOS's kernel, as the owner decided in ADR-009
# (2026-11-27): lwIP, the cyw43 driver and the Pico SDK's own code. At -O0 the Pico W's image missed its slot by 10 KB;
# the kernel stays at the build's own level, the one third-party code stepped through
file(GLOB_RECURSE THIRD_PARTY_OS_SOURCES ${PICO_SDK_PATH}/lib/lwip/src/*.c ${PICO_SDK_PATH}/lib/cyw43-driver/src/*.c
     ${PICO_SDK_PATH}/src/*.c)
# In the image only: the Pico W's bootloader (src/boards/common/flash_layout.cmake) compiles the SDK's sources at the build's own
# level, as ADR-004 proved it
set_source_files_properties(${THIRD_PARTY_OS_SOURCES} PROPERTIES
                            COMPILE_OPTIONS "$<$<STREQUAL:$<TARGET_PROPERTY:NAME>,${TARGET_NAME}>:-Os>")

# The roots the unit trusts, as one PEM text (https_client.c): GitHub's two (src/services/update_fetch/github_roots.pem). A test build
# (-DHTTPS_TEST_CA=ON) trusts the local OTA server's test roots in their place (tools/ota_server.py --make-certs),
# two of the same kinds, so a fetch on the bench holds what a user's does (ADR-008: with all four, 3.3 KB more); no
# other build does, and a test build reaches no GitHub, as the bench rules ask
option(HTTPS_TEST_CA "Test build only: trust the local OTA server's test roots in place of GitHub's" OFF)
set(HTTPS_CA_FILES ${SRC_DIRECTORY}/services/update_fetch/github_roots.pem)
if (HTTPS_TEST_CA)
    set(HTTPS_CA_FILES ${CMAKE_SOURCE_DIR}/logs/ota_server/certs/ecc_root.pem
                       ${CMAKE_SOURCE_DIR}/logs/ota_server/certs/rsa_root.pem)
endif()
set(https_ca_text "")
foreach(ca_file IN LISTS HTTPS_CA_FILES)
    if (NOT EXISTS ${ca_file})
        message(FATAL_ERROR "${ca_file} is missing: run tools/ota_server.py --make-certs --host <this PC's address>")
    endif()
    file(READ ${ca_file} ca_pem)
    string(APPEND https_ca_text "${ca_pem}")
endforeach()
string(REPLACE "\r" "" https_ca_text "${https_ca_text}")
string(REPLACE "\n" "\\n\"\n    \"" https_ca_text "${https_ca_text}")
file(CONFIGURE OUTPUT ${CMAKE_BINARY_DIR}/https/https_ca.h
     CONTENT "// Made by CMakeLists.txt from HTTPS_CA_FILES\nstatic const char https_ca_pem[] =\n    \"${https_ca_text}\";\n"
     @ONLY)
target_include_directories("${TARGET_NAME}" PRIVATE ${CMAKE_BINARY_DIR}/https)

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
# set( CMAKE_VERBOSE_MAKEFILE on )

# The flash layout and the image's metadata block (ADR-004)
include(${SRC_DIRECTORY}/boards/common/flash_layout.cmake)

# Generate extra outputs
pico_add_extra_outputs("${TARGET_NAME}")
