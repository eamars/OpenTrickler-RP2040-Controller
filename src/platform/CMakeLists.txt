# The platform's interfaces (ADR-006): what the application asks of the chip, each a header in include/. Headers
# only; the chip's side is a module of its own beside this file (rp2/)
ot_module(INCLUDE_DIRS include
          REQUIRES freertos)
