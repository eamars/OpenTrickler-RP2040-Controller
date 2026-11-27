# The Pico W on the controller board, with the mini 12864: the RP2040, 2 MB of flash, our bootloader and two slots
# (ADR-006). What a board carries is said here: its modules, and later each module's optimisation (gate 9).
include(${SRC_DIRECTORY}/boards/common/base.cmake)

# This board carries the base build, whole
set(OT_BOARD_MODULES ${OT_BASE_MODULES})
