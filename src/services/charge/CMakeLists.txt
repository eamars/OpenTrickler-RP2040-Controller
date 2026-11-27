ot_module(SRCS charge_mode.c charge_mode_layout.c charge_mode_settings.c
          INCLUDE_DIRS include
          REQUIRES activity events leds storage
          PRIV_REQUIRES charge_control freertos jobs motors platform profile ring scale servo_gate settings)
