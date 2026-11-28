ot_module(SRCS charge_mode.c charge_mode_layout.c charge_mode_settings.c charge_throws.c
          INCLUDE_DIRS include
          REQUIRES activity events leds storage
          PRIV_REQUIRES charge_control charge_profiles freertos gate http_json jobs motors platform profile ring scale
                        settings)
