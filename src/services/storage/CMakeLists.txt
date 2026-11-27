ot_module(SRCS common.c eeprom.c
          INCLUDE_DIRS include
          REQUIRES freertos
          PRIV_REQUIRES cat24c256 system)
