ot_module(SRCS access_point_mode.c setup.c unit_id.c wireless.c
          INCLUDE_DIRS include
          REQUIRES activity
          PRIV_REQUIRES events freertos mbedtls platform settings storage version)
