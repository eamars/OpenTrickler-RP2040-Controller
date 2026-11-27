ot_module(SRCS menu_console.c menu.c menu_nav.c
          INCLUDE_DIRS include
          REQUIRES activity
          PRIV_REQUIRES api calibration charge cleanup events freertos http_server jobs mini_12864 motors profile
                        remote_control rp2_sdk scale servo_gate settings storage system update update_fetch version
                        wireless)
