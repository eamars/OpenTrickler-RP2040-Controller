ot_module(SRCS access_point_mode.c dnsserver.c setup.c unit_id.c wireless.c
          INCLUDE_DIRS include
          REQUIRES activity
          PRIV_REQUIRES events freertos lwip mbedtls rp2 rp2_sdk settings storage version)
