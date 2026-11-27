ot_module(SRCS charge_mode.c charge_mode_layout.c charge_mode_settings.c
          INCLUDE_DIRS include
          REQUIRES activity events leds storage
          PRIV_REQUIRES charge_control freertos jobs menu mini_12864 motors profile ring rp2_sdk scale
                        servo_gate settings uc1701)
