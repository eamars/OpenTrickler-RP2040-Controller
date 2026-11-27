ot_module(SRCS update.c
          INCLUDE_DIRS include
          REQUIRES activity
          PRIV_REQUIRES events freertos http_json jobs mbedtls platform rp2 rp2_sdk storage system version wireless)
