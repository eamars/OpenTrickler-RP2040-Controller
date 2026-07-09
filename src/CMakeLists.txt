# Generate PIO headers
pico_generate_pio_header(app ${SRC_DIRECTORY}/ws2812.pio OUTPUT_DIR ${SRC_DIRECTORY}/generated)
# Generate PIO headers
pico_generate_pio_header(app ${SRC_DIRECTORY}/stepper.pio OUTPUT_DIR ${SRC_DIRECTORY}/generated)

# Find Python, as Python is required to convert pages into source
find_package (Python COMPONENTS Interpreter REQUIRED)

# Generate HTML header
# Reference: https://cliutils.gitlab.io/modern-cmake/chapters/basics/programs.html

# web_portal.html
add_custom_target(
    generate_web_portal_header ALL
    DEPENDS "${SRC_DIRECTORY}/generated/web_portal.html.h"
)

add_custom_command(
    OUTPUT "${SRC_DIRECTORY}/generated/web_portal.html.h"
    DEPENDS "${SRC_DIRECTORY}/html/web_portal.html"
    COMMAND "${Python_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/html2header.py" -vv --no-minify -f ${SRC_DIRECTORY}/html/web_portal.html -o ${SRC_DIRECTORY}/generated/web_portal.html.h
    COMMENT "Generating web_portal.html header"
)

add_dependencies("${TARGET_NAME}" generate_web_portal_header)


# wizard.html
add_custom_target(
    generate_wizard_header ALL
    DEPENDS "${SRC_DIRECTORY}/generated/wizard.html.h"
)

add_custom_command(
    OUTPUT "${SRC_DIRECTORY}/generated/wizard.html.h"
    DEPENDS "${SRC_DIRECTORY}/html/wizard.html"
    COMMAND "${Python_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/html2header.py" -vv --no-minify -f ${SRC_DIRECTORY}/html/wizard.html -o ${SRC_DIRECTORY}/generated/wizard.html.h
    COMMENT "Generating wizard.html header"
)

add_dependencies("${TARGET_NAME}" generate_wizard_header)


# display_mirror.html
add_custom_target(
    generate_display_mirror ALL
    DEPENDS "${SRC_DIRECTORY}/generated/display_mirror.html.h"
)

add_custom_command(
    OUTPUT "${SRC_DIRECTORY}/generated/display_mirror.html.h"
    DEPENDS "${SRC_DIRECTORY}/html/display_mirror.html"
    COMMAND "${Python_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/html2header.py" -vv --no-minify -f ${SRC_DIRECTORY}/html/display_mirror.html -o ${SRC_DIRECTORY}/generated/display_mirror.html.h
    COMMENT "Generating display_mirror.html header"
)

add_dependencies("${TARGET_NAME}" generate_display_mirror)


# Generate version
MESSAGE("Python_EXECUTABLE: ${Python_EXECUTABLE}")
add_custom_command(
    OUTPUT 
        "${SRC_DIRECTORY}/generated/version.c"
        "${SRC_DIRECTORY}/generated/version.h"
        "non_exist_file"
    COMMAND "${Python_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/gen_version.py" -o ${SRC_DIRECTORY}/generated --build-type="${CMAKE_BUILD_TYPE}"
    COMMENT "Generating version"
)
add_library(app_version "${SRC_DIRECTORY}/generated/version.c")


# OTA firmware update: flash layout constants, computed once in the root
# CMakeLists.txt (shared with the bootloader target) and consumed here.
# The app target no longer needs FIRMWARE_UPDATE_APP_REGION_SIZE/OFFSET --
# it never writes to the app region itself (see src/firmware_update.c); only
# the bootloader does, since it's the one that actually copies a staged
# image into place. Kept distinct from the SDK's own PICO_FLASH_SIZE_BYTES
# macro (see root CMakeLists.txt) to avoid colliding with hardware_flash's
# bounds-check against the physical flash size.
# NOTE: values are plain integer literals, not parenthesized expressions --
# CMake silently drops a compile definition whose value starts with '(' when
# passing it on the compiler command line (it's misparsed as a function-style
# macro), so e.g. "(4 * 1024 * 1024)" would never actually reach the compiler.
# FIRMWARE_UPDATE_BATCH_SIZE: how many bytes are erased+programmed per
# flash_safe_execute() call during OTA upload. Each call dynamically creates
# and tears down a max-priority FreeRTOS task on the other core to
# coordinate the multicore lockout (see pico_flash's flash.c) -- doing that
# once per 4KB sector for a large image (hundreds of times back to back) was
# found to wedge the system, so writes are batched into much larger chunks.
# Sized generously on pico2_w (abundant RAM headroom) and more conservatively
# on pico_w (tighter RAM budget) -- must remain a multiple of 4096.
target_compile_definitions("${TARGET_NAME}" PUBLIC
    FIRMWARE_UPDATE_TOTAL_FLASH_SIZE=${FIRMWARE_UPDATE_TOTAL_FLASH_SIZE}
    FIRMWARE_UPDATE_STAGING_OFFSET=${FIRMWARE_UPDATE_STAGING_OFFSET}
    FIRMWARE_UPDATE_STAGING_SIZE=${FIRMWARE_UPDATE_STAGING_SIZE}
    FIRMWARE_UPDATE_BOARD_TYPE=${FIRMWARE_UPDATE_BOARD_TYPE}
    FIRMWARE_UPDATE_BATCH_SIZE=${FIRMWARE_UPDATE_BATCH_SIZE}
)
