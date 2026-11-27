ot_module(SRCS settings.c
          INCLUDE_DIRS include
          REQUIRES activity
          PRIV_REQUIRES api events http_json http_server storage version wireless)
