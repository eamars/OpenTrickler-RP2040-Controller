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
)



# display_mirror.html
add_custom_target(
    generate_display_mirror ALL
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/generated/display_mirror.html.h"
)

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_LIST_DIR}/generated/display_mirror.html.h"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/html/display_mirror.html"
    COMMAND "${PYTHON_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/html2header.py" -vv --no-minify -f ${CMAKE_CURRENT_LIST_DIR}/html/display_mirror.html -o ${CMAKE_CURRENT_LIST_DIR}/generated/display_mirror.html.h
)


# bootstrap.min.css
add_custom_target(
    generate_bootstrap_min_css ALL
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/generated/bootstrap.min.css.h"
)

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_LIST_DIR}/generated/bootstrap.min.css.h"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/html/css/bootstrap.min.css"
    COMMAND "${PYTHON_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/html2header.py" -vv --no-minify -f ${CMAKE_CURRENT_LIST_DIR}/html/css/bootstrap.min.css -o ${CMAKE_CURRENT_LIST_DIR}/generated/bootstrap.min.css.h
)


# bootstrap.min.js
add_custom_target(
    generate_bootstrap_min_js ALL
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/generated/bootstrap.min.js.h"
)

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_LIST_DIR}/generated/bootstrap.min.js.h"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/html/js/bootstrap.min.js"
    COMMAND "${PYTHON_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/html2header.py" -vv --no-minify -f ${CMAKE_CURRENT_LIST_DIR}/html/js/bootstrap.min.js -o ${CMAKE_CURRENT_LIST_DIR}/generated/bootstrap.min.js.h
)


# jquery-3.7.0.min.js
add_custom_target(
    generate_jquery_min_js ALL
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/generated/jquery-3.7.0.min.js.h"
)

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_LIST_DIR}/generated/jquery-3.7.0.min.js.h"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/html/js/jquery-3.7.0.min.js"
    COMMAND "${PYTHON_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/html2header.py" -vv --no-minify -f ${CMAKE_CURRENT_LIST_DIR}/html/js/jquery-3.7.0.min.js -o ${CMAKE_CURRENT_LIST_DIR}/generated/jquery-3.7.0.min.js.h
)


# Generate version
add_custom_target(
    generate_version
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/generated/version.h"
)

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_LIST_DIR}/generated/version.h"
    COMMAND "${PYTHON_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/gen_version.py" -o ${CMAKE_CURRENT_LIST_DIR}/generated/version.h
    WORKING_DIRECTORY ${SCRIPTS_DIRECTORY}
    COMMENT "Generating version"
)

add_dependencies(app generate_version)
