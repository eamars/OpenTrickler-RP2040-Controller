# Generate PIO headers
pico_generate_pio_header(app ${SRC_DIRECTORY}/ws2812.pio OUTPUT_DIR ${SRC_DIRECTORY}/generated)
# Generate PIO headers
pico_generate_pio_header(app ${SRC_DIRECTORY}/stepper.pio OUTPUT_DIR ${SRC_DIRECTORY}/generated)

# Find Python, as Python is required to convert pages into source
find_package (Python COMPONENTS Interpreter REQUIRED)

# Generate HTML header
# Reference: https://cliutils.gitlab.io/modern-cmake/chapters/basics/programs.html


# dashboard.html
add_custom_target(
    generate_dashboard_header ALL
    DEPENDS "${SRC_DIRECTORY}/generated/dashboard.html.h"
)

add_custom_command(
    OUTPUT "${SRC_DIRECTORY}/generated/dashboard.html.h"
    DEPENDS "${SRC_DIRECTORY}/html/dashboard.html"
    COMMAND "${PYTHON_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/html2header.py" -vv --no-minify -f ${SRC_DIRECTORY}/html/dashboard.html -o ${SRC_DIRECTORY}/generated/dashboard.html.h
    COMMENT "Generating dashboard.html header"
)

add_dependencies("${TARGET_NAME}" generate_dashboard_header)


# display_mirror.html
add_custom_target(
    generate_display_mirror ALL
    DEPENDS "${SRC_DIRECTORY}/generated/display_mirror.html.h"
)

add_custom_command(
    OUTPUT "${SRC_DIRECTORY}/generated/display_mirror.html.h"
    DEPENDS "${SRC_DIRECTORY}/html/display_mirror.html"
    COMMAND "${PYTHON_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/html2header.py" -vv --no-minify -f ${SRC_DIRECTORY}/html/display_mirror.html -o ${SRC_DIRECTORY}/generated/display_mirror.html.h
    COMMENT "Generating display_mirror.html header"
)

add_dependencies("${TARGET_NAME}" generate_display_mirror)


# bootstrap.min.css
add_custom_target(
    generate_bootstrap_min_css ALL
    DEPENDS "${SRC_DIRECTORY}/generated/bootstrap.min.css.h"
)

add_custom_command(
    OUTPUT "${SRC_DIRECTORY}/generated/bootstrap.min.css.h"
    DEPENDS "${SRC_DIRECTORY}/html/css/bootstrap.min.css"
    COMMAND "${PYTHON_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/html2header.py" -vv --no-minify -f ${SRC_DIRECTORY}/html/css/bootstrap.min.css -o ${SRC_DIRECTORY}/generated/bootstrap.min.css.h
    COMMENT "Generating boootstrap.min.css header"
)

add_dependencies("${TARGET_NAME}" generate_bootstrap_min_css)


# bootstrap.min.js
add_custom_target(
    generate_bootstrap_min_js ALL
    DEPENDS "${SRC_DIRECTORY}/generated/bootstrap.min.js.h"
)

add_custom_command(
    OUTPUT "${SRC_DIRECTORY}/generated/bootstrap.min.js.h"
    DEPENDS "${SRC_DIRECTORY}/html/js/bootstrap.min.js"
    COMMAND "${PYTHON_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/html2header.py" -vv --no-minify -f ${SRC_DIRECTORY}/html/js/bootstrap.min.js -o ${SRC_DIRECTORY}/generated/bootstrap.min.js.h
    COMMENT "Generating bootstrap.min.js header"
)

add_dependencies("${TARGET_NAME}" generate_bootstrap_min_js)


# jquery-3.7.0.min.js
add_custom_target(
    generate_jquery_min_js ALL
    DEPENDS "${SRC_DIRECTORY}/generated/jquery-3.7.0.min.js.h"
)

add_custom_command(
    OUTPUT "${SRC_DIRECTORY}/generated/jquery-3.7.0.min.js.h"
    DEPENDS "${SRC_DIRECTORY}/html/js/jquery-3.7.0.min.js"
    COMMAND "${PYTHON_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/html2header.py" --no-minify -f ${SRC_DIRECTORY}/html/js/jquery-3.7.0.min.js -o ${SRC_DIRECTORY}/generated/jquery-3.7.0.min.js.h
    COMMENT "Generating jQuery header"
)

add_dependencies("${TARGET_NAME}" generate_jquery_min_js)


# Generate version
add_custom_target(
    generate_version ALL
    DEPENDS "non_exist_file"  # Some workaround to force the re-generation of header regardless of the file update
)

add_custom_command(
    OUTPUT 
        "${SRC_DIRECTORY}/generated/version.c"
        "${SRC_DIRECTORY}/generated/version.h"
        "non_exist_file"
    COMMAND "${PYTHON_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/gen_version.py" -o ${SRC_DIRECTORY}/generated --build-type="${CMAKE_BUILD_TYPE}"
    COMMENT "Generating version"
)
add_dependencies("${TARGET_NAME}" generate_version)
add_library(app_version "${SRC_DIRECTORY}/generated/version.c")
