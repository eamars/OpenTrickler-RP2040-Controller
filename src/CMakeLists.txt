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
