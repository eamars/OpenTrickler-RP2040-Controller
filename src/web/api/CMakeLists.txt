ot_module(SRCS api_v1.c api_v1_commands.c api_v1_settings.c rest_endpoints.c
          INCLUDE_DIRS include
          PRIV_REQUIRES activity calibration charge charge_profiles cleanup events features freertos gate http_json
                        http_server leds lwip
                        motors platform profile scale settings storage system update update_fetch version
                        wireless)

# The guide for an AI agent (ADR-016, gate 2): agent_guide.md as one C string, made as the build configures (the build
# scripts configure each time), served as it is at /api/v1/agent and /llms.txt
set(AGENT_GUIDE_FILE ${CMAKE_CURRENT_LIST_DIR}/agent_guide.md)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${AGENT_GUIDE_FILE})
file(READ ${AGENT_GUIDE_FILE} agent_guide_text)
string(REPLACE "\r" "" agent_guide_text "${agent_guide_text}")
string(REPLACE "\\" "\\\\" agent_guide_text "${agent_guide_text}")
string(REPLACE "\"" "\\\"" agent_guide_text "${agent_guide_text}")
string(REPLACE "\n" "\\n\"\n    \"" agent_guide_text "${agent_guide_text}")
file(CONFIGURE OUTPUT ${CMAKE_BINARY_DIR}/agent/agent_guide.h
     CONTENT "// Made by the api module's CMakeLists.txt from agent_guide.md\nstatic const char agent_guide_text[] =\n    \"${agent_guide_text}\";\n"
     @ONLY)
target_include_directories(ot_api PRIVATE ${CMAKE_BINARY_DIR}/agent)

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
