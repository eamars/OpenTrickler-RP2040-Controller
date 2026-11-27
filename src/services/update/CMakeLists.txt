ot_module(SRCS update.c
          INCLUDE_DIRS include
          REQUIRES activity
          PRIV_REQUIRES api events freertos http_json http_server jobs mbedtls platform rp2 rp2_sdk storage
                        update_fetch version wireless)
