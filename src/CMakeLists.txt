# Generate PIO headers into each board's own build folder, not src/generated: pioasm writes a different
# header for each chip (PIO version 0 on the RP2040, 1 on the RP2350), and a header shared by both builds
# is reused by whichever board builds second (ADR-001, F7)
pico_generate_pio_header(app ${SRC_DIRECTORY}/ws2812.pio)
pico_generate_pio_header(app ${SRC_DIRECTORY}/stepper.pio)

# The headers that stay in src/generated (version, pages). It was on the include path only as a side effect
# of the PIO headers' OUTPUT_DIR
target_include_directories(app PRIVATE ${SRC_DIRECTORY}/generated)

# Find Python, as Python is required to convert pages into source
find_package (Python COMPONENTS Interpreter REQUIRED)

# Generate HTML header
# Reference: https://cliutils.gitlab.io/modern-cmake/chapters/basics/programs.html

# The portal (ADR-008, gate 3): src/web joined, trimmed and gzipped by web_build.py into a header the firmware serves
file(GLOB WEB_SOURCES ${SRC_DIRECTORY}/web/*)
add_custom_target(
    generate_web_portal_header ALL
    DEPENDS "${SRC_DIRECTORY}/generated/web_portal.gz.h"
)

add_custom_command(
    OUTPUT "${SRC_DIRECTORY}/generated/web_portal.gz.h"
    DEPENDS ${WEB_SOURCES} "${SCRIPTS_DIRECTORY}/web_build.py"
    COMMAND "${Python_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/web_build.py" --out ${CMAKE_CURRENT_BINARY_DIR}/web --header ${SRC_DIRECTORY}/generated/web_portal.gz.h
    COMMENT "Generating the portal's header"
)

add_dependencies("${TARGET_NAME}" generate_web_portal_header)






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
# A module that reads the version requires it by name, and so waits for version.h to be made (ADR-006)
target_include_directories(app_version PUBLIC ${SRC_DIRECTORY}/generated)
add_library(ot::version ALIAS app_version)

# ADR-006, gate 1: the build's mechanics tried on two modules, where their files lie. Each names itself, since they
# share this folder until gate 2 gives every module its own; and each gets all of src/ as its include path, so
# nothing is refused yet between our own files
#   - fault_handler: one function, called by nothing of ours, in place of the SDK's weak isr_hardfault. It is in
#     the image only if the module is linked whole
#   - events: a service on FreeRTOS and the SDK, with other modules' headers and a generated one
set(OT_MODULE_NAME fault_handler)
ot_module(SRCS fault_handler.c
          PRIV_INCLUDE_DIRS ${SRC_DIRECTORY}
          PRIV_REQUIRES freertos rp2_sdk)
set(OT_MODULE_NAME events)
ot_module(SRCS events.c
          INCLUDE_DIRS ${SRC_DIRECTORY}
          PRIV_REQUIRES freertos rp2_sdk version)
unset(OT_MODULE_NAME)


# The menu's fonts, made from the BDF files in fonts/ (ADR-010): one family, Helvetica, in three sizes, as the owner
# chose (skill embedded-ui-design): 8 regular for text, 8 bold for titles, 18 bold for the figure a job screen shows.
# The regular one carries the title marks' icons too (gate 5), and the hidden hello's figure is a font of its tiles
set(MENU_FONTS_BDF "${CMAKE_SOURCE_DIR}/fonts")
add_custom_command(
    OUTPUT "${SRC_DIRECTORY}/generated/menu_fonts.c"
    DEPENDS "${SCRIPTS_DIRECTORY}/bdf2lvgl.py" "${MENU_FONTS_BDF}/helvR08.bdf" "${MENU_FONTS_BDF}/helvB08.bdf"
            "${MENU_FONTS_BDF}/helvB18.bdf" "${MENU_FONTS_BDF}/claude_chan.txt"
    COMMAND "${Python_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/bdf2lvgl.py" --out ${SRC_DIRECTORY}/generated/menu_fonts.c
            ${MENU_FONTS_BDF}/helvR08.bdf:menu_font_regular:icons ${MENU_FONTS_BDF}/helvB08.bdf:menu_font_bold
            ${MENU_FONTS_BDF}/helvB18.bdf:menu_font_figure ${MENU_FONTS_BDF}/claude_chan.txt:menu_font_chan:art
    COMMENT "Generating the menu's fonts"
)
add_custom_target(generate_menu_fonts ALL DEPENDS "${SRC_DIRECTORY}/generated/menu_fonts.c")
add_dependencies("${TARGET_NAME}" generate_menu_fonts)
set_source_files_properties("${SRC_DIRECTORY}/generated/menu_fonts.c" PROPERTIES GENERATED TRUE)
target_sources("${TARGET_NAME}" PRIVATE "${SRC_DIRECTORY}/generated/menu_fonts.c")
