# The Pico 2 W on the controller board, with the mini 12864: the RP2350, 4 MB of flash, the ROM's two slots
# (ADR-006). What a board carries is said here: its modules, and each module's optimisation (gate 9).
include(${SRC_DIRECTORY}/boards/common/base.cmake)

# This board carries the base build, whole
set(OT_BOARD_MODULES ${OT_BASE_MODULES})

# Each module's optimisation: ours at the build's own level (-O0 in a Debug build, for debugging) unless named here, as
#   set(OT_MODULE_OPTIONS_<module> -Os)
# one list for the bench's builds and the release's alike (the owner, 2026-11-27). The third-party code's levels are
# in base.cmake. None named today: the image fits its slot (ADR-005's trigger)
