# What both boards share today (ADR-006, gate 3): the third-party code and its settings, the libraries the image
# links, and the base build's modules. A board's own file (boards/<board>/board.cmake) includes this one and says
# what is its own. Included by the root CMakeLists.txt, so everything here is made in the root folder.

# Pull in FreeRTOS. Its location is given, not searched for: upstream's import file (V11.3.3), left to search,
# finds the kernel as the SDK's sibling on its first pass and stops there with that pass's path to the RP2350 port,
# which only Raspberry Pi's fork had. Given the location, it tries both paths and takes the RP2350 port from the
# community ports (ADR-011).
set(FREERTOS_KERNEL_PATH ${FREERTOS_SRC_DIRECTORY})
include(${FREERTOS_SRC_DIRECTORY}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

# The third-party code's settings (FreeRTOSConfig.h, lwipopts.h, mbedtls_config.h), which its own headers and
# sources find by name
set(OT_THIRD_PARTY_SETTINGS ${SRC_DIRECTORY}/boards/common/settings)

# LVGL, configured by the view's lv_conf.h (src/ui/view_12864/lv_conf). Every source is compiled; what lv_conf.h
# switches off compiles to nothing. At -Os in every build, as the owner decided after ADR-010's gate 1: code nobody
# steps through, and at -O0 the Pico W's image did not fit its slot
file(GLOB_RECURSE LVGL_SOURCES ${LVGL_DIRECTORY}/src/*.c)
add_library(lvgl STATIC ${LVGL_SOURCES})
target_include_directories(lvgl PUBLIC ${LVGL_DIRECTORY}/include ${LVGL_DIRECTORY}
                           ${SRC_DIRECTORY}/ui/view_12864/lv_conf)
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)
target_compile_options(lvgl PRIVATE -Os)

# The real libraries, which carry their sources: linked into the image, once
target_include_directories("${TARGET_NAME}" PRIVATE ${OT_THIRD_PARTY_SETTINGS})
target_link_libraries("${TARGET_NAME}"
    pico_stdlib
    hardware_pio
    hardware_spi
    hardware_i2c
    hardware_pwm
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap4
    lvgl

    # Required by cyw43 wireless module
    pico_cyw43_arch_lwip_sys_freertos
    pico_lwip_freertos
    pico_lwip_mdns

    # flash_safe_execute(): flash written with the other core held off it, as the ROM's confirmation of an image
    # does (ADR-004)
    pico_flash
    # mbedTLS, with the boards' mbedtls_config.h: the SHA-256 an image is checked by before it is tried (ADR-004),
    # and the TLS client the update's poll and download fetch by (ADR-010, gate 5; https_client.c)
    pico_mbedtls
)

# Third-party code as a module sees it (ADR-006): its include paths and no source, under ESP-IDF's names for what
# both platforms have, so a module's REQUIRES line is the same under ESP-IDF.
#   - rp2_sdk is the Pico SDK itself, which only platform/rp2 requires.
#   - lwip carries the radio's driver's include paths too (cyw43): only tools/module_graph.py's search keeps a module
#     that requires it from including the driver.
#   - Each brings the settings its headers include by name.
ot_third_party(rp2_sdk pico_stdlib hardware_pio hardware_spi hardware_i2c hardware_pwm hardware_dma pico_flash)
ot_third_party(freertos FreeRTOS-Kernel FreeRTOS-Kernel-Heap4)
ot_third_party(lwip pico_cyw43_arch_lwip_sys_freertos pico_lwip_freertos pico_lwip_mdns)
ot_third_party(mbedtls pico_mbedtls)
# LVGL is a real library with no source in its interface: a module that requires it links it, and so comes before it
# on the link line, where the linker needs it
add_library(ot_lvgl INTERFACE)
add_library(ot::lvgl ALIAS ot_lvgl)
target_link_libraries(ot_lvgl INTERFACE lvgl)
foreach(name freertos lwip mbedtls)
    target_include_directories(ot_${name} INTERFACE ${OT_THIRD_PARTY_SETTINGS})
endforeach()

# The controller board as a module names it: the pin map (configuration.h), plain numbers, the ports by their number
# (ADR-006, gate 7): it carries nothing of the SDK
add_library(ot_board INTERFACE)
add_library(ot::board ALIAS ot_board)
target_include_directories(ot_board INTERFACE ${SRC_DIRECTORY}/boards/common)

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
# But the image's binary information, which says its board: at -Os the compiler puts the SDK's packed items on any
# byte, and a unit on a firmware before ADR-006's gate 10 reads the board only from an item on a word, refusing the
# image as built for another board (the Pico 2 W's, met 2026-11-27)
list(FILTER THIRD_PARTY_OS_SOURCES EXCLUDE REGEX "/pico_standard_binary_info/standard_binary_info\\.c$")
# In the image only: the Pico W's bootloader (flash_layout.cmake) compiles the SDK's sources at the build's own
# level, as ADR-004 proved it. These rules hold for targets made in the root folder, which the image is: the SDK's
# sources compile in the image, and a module compiles none of them
set_source_files_properties(${THIRD_PARTY_OS_SOURCES} PROPERTIES
                            COMPILE_OPTIONS "$<$<STREQUAL:$<TARGET_PROPERTY:NAME>,${TARGET_NAME}>:-Os>")

# The base build: every module the firmware has (the owner, 2026-11-27: everything of today is the base build).
# Each is a folder under src/. A module that requires one not listed here fails as CMake generates
set(OT_BASE_MODULES
    app/app
    ui/menu ui/view_12864 ui/mini_12864
    web/http_server web/api
    console/remote_control
    services/activity services/calibration services/calibration_method services/charge services/charge_control
    services/charge_profiles services/cleanup services/events services/gate services/jobs services/leds services/motors services/profile services/scale
    services/servo_gate services/settings services/storage services/system services/update services/update_fetch
    services/wireless
    drivers/cat24c256 drivers/scale_protocols drivers/tmc2209 drivers/uc1701
    common/http_json common/ring
    platform platform/rp2
)

# The parts a build may leave out (ADR-006, gate 9), each in the list above and on by default: -DFEATURE_<NAME>=OFF
# leaves it out. The servo gate's users are handed it by the start-up's wiring, or nothing (gate.h)
ot_feature(servo_gate services/servo_gate
           "the shutters' servos: GET /api/v1/gate, settings/gate, the screen's Settings, Servo gate")
