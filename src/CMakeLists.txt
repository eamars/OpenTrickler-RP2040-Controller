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


# The menu's fonts, made from the BDF files in fonts/ (ADR-010): one family, Helvetica, in three sizes, as the owner
# chose (skill embedded-ui-design): 8 regular for text, 8 bold for titles, 18 bold for the figure a job screen shows.
# The regular one carries the title marks' icons too (gate 5)
set(MENU_FONTS_BDF "${CMAKE_SOURCE_DIR}/fonts")
add_custom_command(
    OUTPUT "${SRC_DIRECTORY}/generated/menu_fonts.c"
    DEPENDS "${SCRIPTS_DIRECTORY}/bdf2lvgl.py" "${MENU_FONTS_BDF}/helvR08.bdf" "${MENU_FONTS_BDF}/helvB08.bdf"
            "${MENU_FONTS_BDF}/helvB18.bdf"
    COMMAND "${Python_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/bdf2lvgl.py" --out ${SRC_DIRECTORY}/generated/menu_fonts.c
            ${MENU_FONTS_BDF}/helvR08.bdf:menu_font_regular:icons ${MENU_FONTS_BDF}/helvB08.bdf:menu_font_bold
            ${MENU_FONTS_BDF}/helvB18.bdf:menu_font_figure
    COMMENT "Generating the menu's fonts"
)
add_custom_target(generate_menu_fonts ALL DEPENDS "${SRC_DIRECTORY}/generated/menu_fonts.c")
add_dependencies("${TARGET_NAME}" generate_menu_fonts)
set_source_files_properties("${SRC_DIRECTORY}/generated/menu_fonts.c" PROPERTIES GENERATED TRUE)
target_sources("${TARGET_NAME}" PRIVATE "${SRC_DIRECTORY}/generated/menu_fonts.c")
