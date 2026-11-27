ot_module(SRCS common.c eeprom.c
          INCLUDE_DIRS include
          REQUIRES freertos rp2_sdk
          PRIV_REQUIRES cat24c256 system)
