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
# A test build only: a block version of its own, so a trial image built from the running image's commit is newer
set(RC_TEST_BLOCK_VERSION "" CACHE STRING "Test build only: the metadata block's version, first;second, in place of git's")
if (RC_TEST_BLOCK_VERSION)
    set(APP_BLOCK_VERSION "${RC_TEST_BLOCK_VERSION}")
endif()

# An update image (ADR-004) carries the try-before-you-buy mark: it boots once, on a restart that asks for a trial,
# and stays only if it confirms itself. A full install's image carries none, so it boots at once
option(APP_TRIAL_IMAGE "An update image, marked try-before-you-buy" OFF)
if (APP_TRIAL_IMAGE)
    target_compile_definitions("${TARGET_NAME}" PRIVATE PICO_CRT0_IMAGE_TYPE_TBYB=1)
    message("An update image: marked try-before-you-buy")
endif()
if (APP_BLOCK_VERSION)
    list(GET APP_BLOCK_VERSION 0 APP_BLOCK_VERSION_MAJOR)
    list(GET APP_BLOCK_VERSION 1 APP_BLOCK_VERSION_MINOR)
    pico_set_binary_version("${TARGET_NAME}" MAJOR ${APP_BLOCK_VERSION_MAJOR} MINOR ${APP_BLOCK_VERSION_MINOR})
    message("Metadata block version: ${APP_BLOCK_VERSION_MAJOR}.${APP_BLOCK_VERSION_MINOR}")
elseif (APP_TRIAL_IMAGE)
    # An update image with no version is refused by every unit, and a release of them is no release (review Q4). A
    # release's tag is vX.Y.Z (scripts/gen_version.py; vX.Y counts as X.Y.0)
    message(FATAL_ERROR "git gives no version (the nearest tag is not vX.Y.Z): an update image needs one")
else()
    message(WARNING "git gives no version: the image's metadata block carries none")
endif()

if (PICO_RP2350)
    # Pico 2 W: the ROM's partition table names slot A, slot B and a spare part. The image is linked at the start of
    # flash, where the ROM maps whichever slot it boots, and the linker refuses one larger than a slot
    set(APP_FLASH_ORIGIN 0x10000000)
    set(APP_FLASH_LENGTH 1792K)
    set(APP_PARTITION_TABLE_JSON "${SRC_DIRECTORY}/boards/pico2_w/pico2_w_partition_table.json")

    configure_file("${SRC_DIRECTORY}/boards/common/flash_region.ld.in" "${CMAKE_BINARY_DIR}/flash_region/pico_flash_region.ld")
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
elseif (PICO_RP2040)
    # Pico W: our bootloader at the start of flash, then slot A and slot B (src/platform/rp2/include/update_boot.h). The RP2040 runs an
    # image only where it was linked, so an image is linked for one slot: APP_SLOT. A full install is slot A's
    # The bootloader measured 6.4 KB (2026-11-16): 16 KB for it, and 1,016 KB for each slot
    set(UPDATE_LAYOUT_DEFINITIONS UPDATE_SLOT_A_OFFSET=0x4000 UPDATE_SLOT_B_OFFSET=0x102000 UPDATE_SLOT_SIZE=0xFE000)
    set(APP_SLOT "A" CACHE STRING "Pico W: the slot the image is linked for, A or B")
    if (APP_SLOT STREQUAL "B")
        set(APP_FLASH_ORIGIN 0x10102000)
        set(APP_SLOT_INDEX 1)
    elseif (APP_SLOT STREQUAL "A")
        set(APP_FLASH_ORIGIN 0x10004000)
        set(APP_SLOT_INDEX 0)
    else()
        message(FATAL_ERROR "APP_SLOT is A or B, not ${APP_SLOT}")
    endif()
    set(APP_FLASH_LENGTH 1016K)
    message("Linked for slot ${APP_SLOT} at ${APP_FLASH_ORIGIN}")
    configure_file("${SRC_DIRECTORY}/boards/common/flash_region.ld.in" "${CMAKE_BINARY_DIR}/flash_region/pico_flash_region.ld")
    pico_add_linker_script_override_path("${TARGET_NAME}" "${CMAKE_BINARY_DIR}/flash_region" FILES pico_flash_region.ld)
    # The metadata block the RP2350's ROM reads, off by default on the RP2040, and the image sealed with its SHA-256
    target_compile_definitions("${TARGET_NAME}" PRIVATE PICO_CRT0_INCLUDE_PICOBIN_IMAGE_TYPE_ITEM=1
                               ${UPDATE_LAYOUT_DEFINITIONS} APP_SLOT_INDEX=${APP_SLOT_INDEX})
    pico_hash_binary("${TARGET_NAME}")

    # The bootloader: the start of flash, 16 KB at most; its boot2 is the board's
    add_executable(bootloader "${CMAKE_SOURCE_DIR}/bootloader/bootloader.c" "${SRC_DIRECTORY}/platform/rp2/picobin_image.c")
    target_include_directories(bootloader PRIVATE "${SRC_DIRECTORY}/platform/rp2/include")
    target_compile_definitions(bootloader PRIVATE ${UPDATE_LAYOUT_DEFINITIONS})
    target_link_libraries(bootloader pico_runtime hardware_resets hardware_sync hardware_watchdog pico_bootrom)
    target_link_options(bootloader PRIVATE -Wl,--gc-sections -Wl,--print-memory-usage)
    # The SDK's runtime without what the bootloader never uses: printf, floats, the alarm pool, mutexes
    pico_minimize_runtime(bootloader)
    set(APP_FLASH_ORIGIN 0x10000000)
    set(APP_FLASH_LENGTH 16K)
    configure_file("${SRC_DIRECTORY}/boards/common/flash_region.ld.in"
                   "${CMAKE_BINARY_DIR}/bootloader_flash_region/pico_flash_region.ld")
    pico_add_linker_script_override_path(bootloader "${CMAKE_BINARY_DIR}/bootloader_flash_region"
                                         FILES pico_flash_region.ld)
    pico_add_extra_outputs(bootloader)
endif()

# The full install (ADR-004): one .uf2 for the USB drive, made from a full install's image (not an update image,
# and slot A's on the Pico W). It writes the whole flash: what starts it and slot A's image, every other page erased,
# so no other image is kept (the owner). The Pico 2 W's flash is 4 MB, the Pico W's 2 MB
if (NOT APP_TRIAL_IMAGE AND (PICO_RP2350 OR APP_SLOT STREQUAL "A"))
    if (PICO_RP2350)
        set(FULL_INSTALL_ARGS --family absolute --flash 0x10000000:0x400000
            --part "${CMAKE_BINARY_DIR}/partition_table.bin@0x10000000"
            --part "${CMAKE_BINARY_DIR}/${TARGET_NAME}.bin@0x10002000")
        set(FULL_INSTALL_DEPENDS "${TARGET_NAME}" partition_table)
    else()
        set(FULL_INSTALL_ARGS --family rp2040 --flash 0x10000000:0x200000
            --part "${CMAKE_BINARY_DIR}/bootloader.bin@0x10000000"
            --part "${CMAKE_BINARY_DIR}/${TARGET_NAME}.bin@0x10004000")
        set(FULL_INSTALL_DEPENDS "${TARGET_NAME}" bootloader)
    endif()
    add_custom_target(full_install ALL
        COMMAND "${Python_EXECUTABLE}" "${SCRIPTS_DIRECTORY}/full_install_uf2.py" ${FULL_INSTALL_ARGS}
                --out "${CMAKE_BINARY_DIR}/full_install.uf2"
        COMMENT "Making the full install"
        VERBATIM
    )
    add_dependencies(full_install ${FULL_INSTALL_DEPENDS})
endif()
