# What a build may leave out (ADR-005; -DFEATURE_SERVO_GATE=OFF): its header stays, with what it gives a build without
# the gate, and its source goes
if (FEATURE_SERVO_GATE)
    ot_module(SRCS servo_gate.c
              INCLUDE_DIRS include
              REQUIRES freertos
              PRIV_REQUIRES board rp2_sdk settings storage)
else()
    ot_module(INCLUDE_DIRS include
              REQUIRES freertos)
endif()
