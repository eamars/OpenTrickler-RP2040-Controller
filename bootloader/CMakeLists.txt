# Second-stage OTA bootloader: a small, separate, single-threaded executable
# that runs before the main "app" target on every boot. See main.c for what
# it does and why. Consumes the OTA flash layout variables set in the root
# CMakeLists.txt, before this subdirectory is added.

add_executable(bootloader
    main.c
)

target_include_directories(bootloader PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_include_directories(bootloader PUBLIC ${CMAKE_SOURCE_DIR}/targets)

target_link_libraries(bootloader
    pico_stdlib
    hardware_flash
    hardware_resets
    pico_bootrom
)

target_compile_definitions(bootloader PRIVATE
    FIRMWARE_UPDATE_STAGING_OFFSET=${FIRMWARE_UPDATE_STAGING_OFFSET}
    FIRMWARE_UPDATE_STAGING_SIZE=${FIRMWARE_UPDATE_STAGING_SIZE}
    FIRMWARE_UPDATE_APP_REGION_OFFSET=${FIRMWARE_UPDATE_APP_REGION_OFFSET}
    FIRMWARE_UPDATE_APP_REGION_SIZE=${FIRMWARE_UPDATE_APP_REGION_SIZE}
    FIRMWARE_UPDATE_BOARD_TYPE=${FIRMWARE_UPDATE_BOARD_TYPE}
)

target_link_options(bootloader PUBLIC -Wl,--gc-sections -Wl,--print-memory-usage)

pico_add_extra_outputs(bootloader)

# Custom, self-contained flash region for this target, derived fresh (at
# configure time) from the SDK's own per-platform default memmap template,
# with only the MEMORY block's FLASH region replaced. A custom linker script
# (rather than the shared pico_flash_region.ld override the "app" target
# uses, see root CMakeLists.txt) is required here because both targets link
# against pico_standard_link, which only generates ONE shared
# pico_flash_region.ld per build directory -- two targets needing two
# different FLASH regions can't both come from that one shared, global file.
# Deriving this from the SDK's live template (rather than a hand-maintained
# copy) means it can't silently drift out of sync with the vendored SDK.
file(READ "${PICO_LINKER_SCRIPT_PATH}/memmap_default.ld" _bootloader_memmap_template)
string(REPLACE
    "INCLUDE \"pico_flash_region.ld\""
    "FLASH(rx) : ORIGIN = 0x10000000, LENGTH = ${BOOTLOADER_FLASH_SIZE_BYTES}"
    _bootloader_memmap_content
    "${_bootloader_memmap_template}"
)
file(WRITE "${CMAKE_BINARY_DIR}/memmap_bootloader.ld" "${_bootloader_memmap_content}")
pico_set_linker_script(bootloader "${CMAKE_BINARY_DIR}/memmap_bootloader.ld")
