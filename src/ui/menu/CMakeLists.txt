ot_module(SRCS menu_console.c menu.c menu_nav.c
          INCLUDE_DIRS include
          REQUIRES activity
          PRIV_REQUIRES api calibration charge cleanup events freertos http_server jobs lvgl mini_12864 motors
                        profile remote_control rp2_sdk scale servo_gate settings storage system uc1701 update
                        update_fetch version view_12864 wireless)
