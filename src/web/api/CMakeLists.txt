ot_module(SRCS api_v1.c api_v1_commands.c api_v1_settings.c rest_endpoints.c
          INCLUDE_DIRS include
          PRIV_REQUIRES activity calibration charge cleanup events freertos http_json http_server leds lwip motors
                        profile rp2_sdk scale servo_gate settings storage system update update_fetch version
                        wireless)

# The portal (ADR-008, gate 3): portal/ joined, trimmed and gzipped by web_build.py into a header this module serves
file(GLOB WEB_SOURCES ${CMAKE_CURRENT_LIST_DIR}/portal/*)
add_custom_command(
    OUTPUT "${SRC_DIRECTORY}/generated/web_portal.gz.h"
    DEPENDS ${WEB_SOURCES} "${SCRIPTS_DIRECTORY}/web_build.py"
    COMMAND "${Python_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/web_build.py" --out ${CMAKE_CURRENT_BINARY_DIR}/web --header ${SRC_DIRECTORY}/generated/web_portal.gz.h
    COMMENT "Generating the portal's header"
)
add_custom_target(generate_web_portal_header ALL DEPENDS "${SRC_DIRECTORY}/generated/web_portal.gz.h")
add_dependencies(ot_api generate_web_portal_header)
target_include_directories(ot_api PRIVATE ${SRC_DIRECTORY}/generated)
