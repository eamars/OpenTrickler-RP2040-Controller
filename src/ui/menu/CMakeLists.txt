ot_module(SRCS menu_console.c menu.c menu_nav.c
          INCLUDE_DIRS include
          REQUIRES activity
          PRIV_REQUIRES api calibration charge charge_profiles cleanup events freertos gate http_server jobs mini_12864
                        motors platform
                        profile remote_control scale settings storage system update update_fetch version
                        wireless)
