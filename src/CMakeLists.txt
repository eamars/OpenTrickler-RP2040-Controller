# Generate PIO headers
pico_generate_pio_header(app_base ${SRC_DIRECTORY}/ws2812.pio OUTPUT_DIR ${SRC_DIRECTORY}/generated)
# Generate PIO headers
pico_generate_pio_header(app_base ${SRC_DIRECTORY}/stepper.pio OUTPUT_DIR ${SRC_DIRECTORY}/generated)

# Find Python, as Python is required to convert pages into source
find_package (Python COMPONENTS Interpreter REQUIRED)
MESSAGE("Python_EXECUTABLE: ${Python_EXECUTABLE}")

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

add_dependencies(app_base generate_web_portal_header)


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

add_dependencies(app_base generate_wizard_header)


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

add_dependencies(app_base generate_display_mirror)


# Generate two app headers. The target will select the one to linked against
add_custom_command(
    OUTPUT 
        "${SRC_DIRECTORY}/generated/app_header_0.c"
        "non_exist_file0"
    COMMAND "${Python_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/gen_app_header.py" -o ${SRC_DIRECTORY}/generated --build-type="${CMAKE_BUILD_TYPE}" --flash-partition-idx=0
    COMMENT "Generating version"
)
add_library(app_version_0 INTERFACE)
target_include_directories(app_version_0 INTERFACE ${SRC_DIRECTORY})
target_sources(app_version_0 INTERFACE "${SRC_DIRECTORY}/generated/app_header_0.c")


add_custom_command(
    OUTPUT 
        "${SRC_DIRECTORY}/generated/app_header_1.c"
        "non_exist_file1"
    COMMAND "${Python_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/gen_app_header.py" -o ${SRC_DIRECTORY}/generated --build-type="${CMAKE_BUILD_TYPE}" --flash-partition-idx=1
    COMMENT "Generating version"
)
add_library(app_version_1 INTERFACE)
target_include_directories(app_version_1 INTERFACE ${SRC_DIRECTORY})
target_sources(app_version_1 INTERFACE "${SRC_DIRECTORY}/generated/app_header_0.c")
