# The flash layout and the image's metadata block (ADR-004): two image slots on each board, and the version the
# boot path compares, in the block the SDK's start-up code writes

# The block's version, from the same `git describe` as the firmware's version string: the release (major x 256 +
# minor) and the commits since its tag. Taken when CMake configures, as the build scripts and CI do at every build;
# `rc_run.py flash` checks the two agree before it writes an image
find_package(Python COMPONENTS Interpreter REQUIRED)
execute_process(
    COMMAND "${Python_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/gen_version.py" --block-version
    OUTPUT_VARIABLE APP_BLOCK_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
)
if (APP_BLOCK_VERSION)
    list(GET APP_BLOCK_VERSION 0 APP_BLOCK_VERSION_MAJOR)
    list(GET APP_BLOCK_VERSION 1 APP_BLOCK_VERSION_MINOR)
    pico_set_binary_version("${TARGET_NAME}" MAJOR ${APP_BLOCK_VERSION_MAJOR} MINOR ${APP_BLOCK_VERSION_MINOR})
    message("Metadata block version: ${APP_BLOCK_VERSION_MAJOR}.${APP_BLOCK_VERSION_MINOR}")
else()
    message(WARNING "git gives no version: the image's metadata block carries none")
endif()

if (PICO_RP2350)
    # Pico 2 W: the ROM's partition table names slot A, slot B and a spare part. The image is linked at the start of
    # flash, where the ROM maps whichever slot it boots, and the linker refuses one larger than a slot
    set(APP_FLASH_ORIGIN 0x10000000)
    set(APP_FLASH_LENGTH 1792K)
    set(APP_PARTITION_TABLE_JSON "${CMAKE_SOURCE_DIR}/targets/pico2_w_partition_table.json")

    configure_file("${CMAKE_SOURCE_DIR}/targets/flash_region.ld.in" "${CMAKE_BINARY_DIR}/flash_region/pico_flash_region.ld")
    pico_add_linker_script_override_path("${TARGET_NAME}" "${CMAKE_BINARY_DIR}/flash_region" FILES pico_flash_region.ld)

    # The image sealed with its SHA-256, checked before an image is booted on trial (whether the ROM checks it at
    # every boot is shown on the chip in gate 1)
    pico_hash_binary("${TARGET_NAME}")

    # The partition table as bytes for the start of flash: the probe writes it (rc_run.py flash), and so will the
    # full install
    pico_init_picotool()
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/partition_table.bin"
        COMMAND picotool partition create --quiet "${APP_PARTITION_TABLE_JSON}" "${CMAKE_BINARY_DIR}/partition_table.bin"
        DEPENDS "${APP_PARTITION_TABLE_JSON}"
        COMMENT "Making the partition table"
    )
    add_custom_target(partition_table ALL DEPENDS "${CMAKE_BINARY_DIR}/partition_table.bin")
endif()
