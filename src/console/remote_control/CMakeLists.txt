ot_module(SRCS rc_console.c remote_control.c
          INCLUDE_DIRS include
          PRIV_REQUIRES activity calibration charge freertos http_server lwip menu mini_12864 motors profile
                        rp2_sdk scale settings storage uc1701 update version wireless)
