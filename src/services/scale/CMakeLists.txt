ot_module(SRCS scale.c simulated_scale.c
          INCLUDE_DIRS include
          REQUIRES freertos
          PRIV_REQUIRES board charge motors rp2_sdk settings storage)
