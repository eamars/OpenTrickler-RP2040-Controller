ot_module(SRCS scale.c scale_reader.c simulated_scale.c
          INCLUDE_DIRS include
          REQUIRES freertos scale_protocols
          PRIV_REQUIRES board motors platform settings storage)
