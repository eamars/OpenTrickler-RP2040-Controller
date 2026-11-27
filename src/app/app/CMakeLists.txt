ot_module(SRCS app.c
          INCLUDE_DIRS include
          REQUIRES storage
          PRIV_REQUIRES activity api board calibration charge cleanup events freertos http_server jobs leds menu
                        mini_12864 motors profile remote_control rp2_sdk scale servo_gate uc1701 update
                        update_fetch view_12864 wireless)

# The version (scripts/gen_version.py), made at every build into src/generated. A module that reads it requires it
# by the name `version`, and so waits for version.h to be made
add_custom_command(
    OUTPUT
        "${SRC_DIRECTORY}/generated/version.c"
        "${SRC_DIRECTORY}/generated/version.h"
        "non_exist_file"
    COMMAND "${Python_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/gen_version.py" -o ${SRC_DIRECTORY}/generated --build-type="${CMAKE_BUILD_TYPE}"
    COMMENT "Generating version"
)
add_library(app_version "${SRC_DIRECTORY}/generated/version.c")
target_include_directories(app_version PUBLIC ${SRC_DIRECTORY}/generated)
add_library(ot::version ALIAS app_version)
