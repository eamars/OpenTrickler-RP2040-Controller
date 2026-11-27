ot_module(SRCS rc_console.c remote_control.c
          INCLUDE_DIRS include
          PRIV_REQUIRES activity calibration charge freertos motors profile rp2_sdk scale settings storage system
                        update version wireless)
