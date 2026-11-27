ot_module(SRCS update.c
          INCLUDE_DIRS include
          REQUIRES activity
          PRIV_REQUIRES api events freertos http_json http_server jobs mbedtls mini_12864 platform rp2 rp2_sdk
                        storage uc1701 update_fetch version wireless)
