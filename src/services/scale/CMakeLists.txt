ot_module(SRCS scale.c simulated_scale.c
          INCLUDE_DIRS include
          REQUIRES freertos scale_protocols
          PRIV_REQUIRES board motors rp2_sdk settings storage)
