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
    COMMENT "Generating app header 0"
)
add_library(app_header_0)
target_include_directories(app_header_0 PRIVATE ${SRC_DIRECTORY})
target_sources(app_header_0 PRIVATE "${SRC_DIRECTORY}/generated/app_header_0.c")


add_custom_command(
    OUTPUT 
        "${SRC_DIRECTORY}/generated/app_header_1.c"
        "non_exist_file1"
    COMMAND "${Python_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/gen_app_header.py" -o ${SRC_DIRECTORY}/generated --build-type="${CMAKE_BUILD_TYPE}" --flash-partition-idx=1
    COMMENT "Generating app header 1"
)
add_library(app_header_1)
target_include_directories(app_header_1 PRIVATE ${SRC_DIRECTORY})
target_sources(app_header_1 PRIVATE "${SRC_DIRECTORY}/generated/app_header_1.c")


## Add modules
# Common
add_library(common INTERFACE)
target_include_directories(common INTERFACE ${SRC_DIRECTORY})
target_sources(common INTERFACE "${SRC_DIRECTORY}/common.c")

# Access point mode
add_library(access_point_mode INTERFACE)
target_include_directories(access_point_mode INTERFACE ${SRC_DIRECTORY})
target_sources(access_point_mode INTERFACE "${SRC_DIRECTORY}/access_point_mode.c")

# G&G Scale
add_library(gng_scale INTERFACE)
target_include_directories(gng_scale INTERFACE ${SRC_DIRECTORY})
target_sources(gng_scale INTERFACE 
    "${SRC_DIRECTORY}/gng_scale.c"
)

# A&D Scale
add_library(and_scale INTERFACE)
target_include_directories(and_scale INTERFACE ${SRC_DIRECTORY})
target_sources(and_scale INTERFACE 
    "${SRC_DIRECTORY}/and_scale_calibrate.c"
    "${SRC_DIRECTORY}/and_scale.c"
)

# Steinberg scale
add_library(steinberg_scale INTERFACE)
target_include_directories(steinberg_scale INTERFACE ${SRC_DIRECTORY})
target_sources(steinberg_scale INTERFACE 
    "${SRC_DIRECTORY}/steinberg_scale.c"
)

# Scale (abstract class)
add_library(scale INTERFACE)
target_include_directories(scale INTERFACE ${SRC_DIRECTORY})
target_sources(scale INTERFACE 
    "${SRC_DIRECTORY}/scale.c"
)

# Button
add_library(button INTERFACE)
target_include_directories(button INTERFACE ${SRC_DIRECTORY})
target_sources(button INTERFACE 
    "${SRC_DIRECTORY}/button.c"
    "${SRC_DIRECTORY}/rotary_button.cpp"
)

# Charge mode
add_library(charge_mode INTERFACE)
target_include_directories(charge_mode INTERFACE ${SRC_DIRECTORY})
target_sources(charge_mode INTERFACE 
    "${SRC_DIRECTORY}/charge_mode.cpp"
    "${SRC_DIRECTORY}/FloatRingBuffer.cpp"
)

# Cleanup mode
add_library(cleanup_mode INTERFACE)
target_include_directories(cleanup_mode INTERFACE ${SRC_DIRECTORY})
target_sources(cleanup_mode INTERFACE 
    "${SRC_DIRECTORY}/cleanup_mode.cpp"
)

# HTTP server
add_library(http_server INTERFACE)
target_include_directories(http_server INTERFACE ${SRC_DIRECTORY})
target_sources(http_server INTERFACE 
    "${SRC_DIRECTORY}/dhcpserver.c"
    "${SRC_DIRECTORY}/dnsserver.c"
    "${SRC_DIRECTORY}/http_rest.c"
)

# REST server
add_library(rest_server INTERFACE)
target_include_directories(rest_server INTERFACE ${SRC_DIRECTORY})
target_sources(rest_server INTERFACE 
    "${SRC_DIRECTORY}/rest_endpoints.c"
    "${SRC_DIRECTORY}/rest_app_control.c"

)

# EEPROM
add_library(eeprom INTERFACE)
target_include_directories(eeprom INTERFACE ${SRC_DIRECTORY})
target_sources(eeprom INTERFACE 
    "${SRC_DIRECTORY}/eeprom.c"
    "${SRC_DIRECTORY}/cat24c256_eeprom.c"
)

# Menu
add_library(menu INTERFACE)
target_include_directories(menu INTERFACE ${SRC_DIRECTORY})
target_sources(menu INTERFACE 
    "${SRC_DIRECTORY}/menu.cpp"
    "${SRC_DIRECTORY}/mui_menu.c"
)

# Display
add_library(display INTERFACE)
target_include_directories(display INTERFACE ${SRC_DIRECTORY})
target_sources(display INTERFACE 
    "${SRC_DIRECTORY}/display.c"
    "${SRC_DIRECTORY}/uc1701_display.c"
)

# Motors
add_library(motors INTERFACE)
target_include_directories(motors INTERFACE ${SRC_DIRECTORY})
target_sources(motors INTERFACE 
    "${SRC_DIRECTORY}/motors.c"
)

# Neopixel
add_library(neopixel INTERFACE)
target_include_directories(neopixel INTERFACE ${SRC_DIRECTORY})
target_sources(neopixel INTERFACE 
    "${SRC_DIRECTORY}/neopixel_led.c"
)

# Profile
add_library(profile INTERFACE)
target_include_directories(profile INTERFACE ${SRC_DIRECTORY})
target_sources(profile INTERFACE 
    "${SRC_DIRECTORY}/profile.c"
)

# Wireless
add_library(wireless INTERFACE)
target_include_directories(wireless INTERFACE ${SRC_DIRECTORY})
target_sources(wireless INTERFACE 
    "${SRC_DIRECTORY}/wireless.c"
)
