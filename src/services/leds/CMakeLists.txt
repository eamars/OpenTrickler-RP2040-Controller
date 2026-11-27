ot_module(SRCS neopixel_led.c
          INCLUDE_DIRS include
          REQUIRES freertos storage
          PRIV_REQUIRES activity board platform settings)
