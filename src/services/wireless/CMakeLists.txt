ot_module(SRCS access_point_mode.c dnsserver.c setup.c unit_id.c wireless.c
          INCLUDE_DIRS include
          REQUIRES activity
          PRIV_REQUIRES api events freertos http_server lwip mbedtls mini_12864 rp2 rp2_sdk settings storage
                        uc1701 update update_fetch version)
