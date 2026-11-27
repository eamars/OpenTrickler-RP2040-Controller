ot_module(SRCS button.c mini_12864_module.cpp mini_12864_settings.c
          INCLUDE_DIRS include
          PRIV_REQUIRES board freertos platform rp2 rp2_sdk settings storage uc1701)
