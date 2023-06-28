# Generate PIO headers
pico_generate_pio_header(app ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)
# Generate PIO headers
pico_generate_pio_header(app ${CMAKE_CURRENT_LIST_DIR}/stepper.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

# Generate HTML header
# Reference: https://cliutils.gitlab.io/modern-cmake/chapters/basics/programs.html
find_package(PythonInterp REQUIRED)


# dashboard.html
add_custom_target(
    generate_dashboard_header ALL
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/generated/dashboard.html.h"
)

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_LIST_DIR}/generated/dashboard.html.h"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/html/dashboard.html"
    COMMAND "${PYTHON_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/html2header.py" -vv --no-minify -f ${CMAKE_CURRENT_LIST_DIR}/html/dashboard.html -o ${CMAKE_CURRENT_LIST_DIR}/generated/dashboard.html.h
    COMMENT "Generating dashboard.html header"
)

add_dependencies(app generate_dashboard_header)


# display_mirror.html
add_custom_target(
    generate_display_mirror ALL
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/generated/display_mirror.html.h"
)

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_LIST_DIR}/generated/display_mirror.html.h"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/html/display_mirror.html"
    COMMAND "${PYTHON_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/html2header.py" -vv --no-minify -f ${CMAKE_CURRENT_LIST_DIR}/html/display_mirror.html -o ${CMAKE_CURRENT_LIST_DIR}/generated/display_mirror.html.h
    COMMENT "Generating display_mirror.html header"
)

add_dependencies(app generate_display_mirror)


# bootstrap.min.css
add_custom_target(
    generate_bootstrap_min_css ALL
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/generated/bootstrap.min.css.h"
)

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_LIST_DIR}/generated/bootstrap.min.css.h"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/html/css/bootstrap.min.css"
    COMMAND "${PYTHON_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/html2header.py" -vv --no-minify -f ${CMAKE_CURRENT_LIST_DIR}/html/css/bootstrap.min.css -o ${CMAKE_CURRENT_LIST_DIR}/generated/bootstrap.min.css.h
    COMMENT "Generating boootstrap.min.css header"
)

add_dependencies(app generate_bootstrap_min_css)


# bootstrap.min.js
add_custom_target(
    generate_bootstrap_min_js ALL
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/generated/bootstrap.min.js.h"
)

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_LIST_DIR}/generated/bootstrap.min.js.h"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/html/js/bootstrap.min.js"
    COMMAND "${PYTHON_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/html2header.py" -vv --no-minify -f ${CMAKE_CURRENT_LIST_DIR}/html/js/bootstrap.min.js -o ${CMAKE_CURRENT_LIST_DIR}/generated/bootstrap.min.js.h
    COMMENT "Generating bootstrap.min.js header"
)

add_dependencies(app generate_bootstrap_min_js)


# jquery-3.7.0.min.js
add_custom_target(
    generate_jquery_min_js ALL
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/generated/jquery-3.7.0.min.js.h"
)

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_LIST_DIR}/generated/jquery-3.7.0.min.js.h"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/html/js/jquery-3.7.0.min.js"
    COMMAND "${PYTHON_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/html2header.py" --no-minify -f ${CMAKE_CURRENT_LIST_DIR}/html/js/jquery-3.7.0.min.js -o ${CMAKE_CURRENT_LIST_DIR}/generated/jquery-3.7.0.min.js.h
    COMMENT "Generating jQuery header"
)

add_dependencies(app generate_jquery_min_js)


# Generate version
add_custom_target(
    generate_version ALL
    DEPENDS "non_exist_file"  # Some workaround to force the re-generation of header regardless of the file update
)

add_custom_command(
    OUTPUT 
        "${CMAKE_CURRENT_LIST_DIR}/generated/version.h"
        "${CMAKE_CURRENT_LIST_DIR}/generated/version.c"
        "non_exist_file"
    COMMAND "${PYTHON_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/gen_version.py" -o ${CMAKE_CURRENT_LIST_DIR}/generated --build-type="${CMAKE_BUILD_TYPE}"
    COMMENT "Generating version"
)

add_dependencies(app generate_version)
