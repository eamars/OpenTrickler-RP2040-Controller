# The PC build (ADR-006, gate 4): the firmware's modules that need no chip, built by this PC's compiler from their
# own build files, for the host tests (tests/host_test.py, step2_sim.py, calibration_sim.py). It has no product and
# turns no motor: the PC is never a board.
#
# tests/host_test.py configures and builds it (host_build()); nothing here is run by hand.
cmake_minimum_required(VERSION 3.25)
project(opentrickler_host C)

get_filename_component(REPO "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(SRC_DIRECTORY ${REPO}/src)

# `zig cc` is a compiler with no archiver of its own name: its archiver is `zig ar`
if (CMAKE_C_COMPILER MATCHES "zig")
    set(CMAKE_C_ARCHIVE_CREATE "\"${CMAKE_C_COMPILER}\" ar qc <TARGET> <OBJECTS>")
    set(CMAKE_C_ARCHIVE_FINISH "")
endif()

# What everything here compiles with. -fPIC: two of the tests are libraries Python loads
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_compile_options(-std=c11 -Wall -O1)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---- The module call, for the PC ----
# The same call as the firmware's (cmake/ot_module.cmake), made by the PC's compiler: a module is a static library,
# and sees only the headers of what it names. Nothing is linked whole: a test links what it calls.
#
# A module not yet free of the chip is taken in part (HOST_PARTS, below): its headers and what they require, from
# its own build file, and of its sources only those named here. What only its sources require is left out, with
# the sources.
function(ot_module)
    cmake_parse_arguments(PARSE_ARGV 0 arg "" "" "SRCS;INCLUDE_DIRS;PRIV_INCLUDE_DIRS;REQUIRES;PRIV_REQUIRES")
    if (arg_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "ot_module: not an argument of idf_component_register: ${arg_UNPARSED_ARGUMENTS}")
    endif()
    get_filename_component(name "${CMAKE_CURRENT_LIST_DIR}" NAME)
    if (DEFINED HOST_PART_${name})
        set(arg_SRCS ${HOST_PART_${name}})
        set(arg_PRIV_INCLUDE_DIRS "")
        set(arg_PRIV_REQUIRES "")
    endif()
    list(TRANSFORM arg_REQUIRES PREPEND ot::)
    list(TRANSFORM arg_PRIV_REQUIRES PREPEND ot::)

    if (arg_SRCS)
        add_library(ot_${name} STATIC ${arg_SRCS})
        target_include_directories(ot_${name} PUBLIC ${arg_INCLUDE_DIRS} PRIVATE ${arg_PRIV_INCLUDE_DIRS})
        target_link_libraries(ot_${name} PUBLIC ${arg_REQUIRES} PRIVATE ${arg_PRIV_REQUIRES})
    else()
        add_library(ot_${name} INTERFACE)
        target_include_directories(ot_${name} INTERFACE ${arg_INCLUDE_DIRS})
        target_link_libraries(ot_${name} INTERFACE ${arg_REQUIRES})
    endif()
    add_library(ot::${name} ALIAS ot_${name})
endfunction()

# ---- The stand-ins ----
# Third-party code a module names, as the PC has it: headers with only what the files built here use. They are this
# build's meter (ADR-006): three of FreeRTOS's and three of the Pico SDK's at gate 4; at the ADR's end the SDK's
# are gone.
foreach(name freertos rp2_sdk)
    add_library(ot_${name} INTERFACE)
    target_include_directories(ot_${name} INTERFACE stand_ins/${name})
    add_library(ot::${name} ALIAS ot_${name})
endforeach()

# ---- The modules ----
# Whole: every module that requires nothing of the chip or the RTOS. One that comes to include an SDK header no
# longer builds here. (common/ring joins when it is C: gate 5.)
set(HOST_MODULES
    common/http_json
    drivers/tmc2209
    services/calibration_method
    services/charge_control
)

# In part: modules not yet free, of which a test needs the stored records' side. For each, the sources taken;
# none for a module whose headers alone are needed. The list shortens as gates 6 to 8 free them.
set(HOST_PART_storage common.c)                 # the loader
set(HOST_PART_charge charge_mode_layout.c)      # the charge record's earlier layouts
set(HOST_PART_profile "")
set(HOST_PART_activity "")
set(HOST_PART_events "")
set(HOST_PART_leds "")
set(HOST_PARTS
    services/storage
    services/charge
    services/profile
    services/activity
    services/events
    services/leds
)

foreach(module IN LISTS HOST_MODULES HOST_PARTS)
    add_subdirectory(${SRC_DIRECTORY}/${module} modules/${module})
endforeach()

# ---- The tests ----
# The loader keeping an earlier version's settings (ADR-009, gate 2), over an EEPROM in RAM
add_executable(config_migration_test config_migration_test.c fake_eeprom.c)
target_link_libraries(config_migration_test PRIVATE ot::storage ot::charge ot::profile m)

# The charge controller and the tuner as a library the simulation loads (tests/step2_sim.py)
add_library(charge_lib SHARED charge_lib.c)
target_link_libraries(charge_lib PRIVATE ot::charge_control)
# These two build with no warning at all, as the simulation has always asked of them
target_compile_options(ot_charge_control PRIVATE -Wextra -Werror)
target_compile_options(charge_lib PRIVATE -Wextra -Werror)

# The calibration's method, the same way (tests/calibration_sim.py)
add_library(calibration_lib SHARED calibration_lib.c)
target_link_libraries(calibration_lib PRIVATE ot::calibration_method)

set_target_properties(charge_lib calibration_lib PROPERTIES PREFIX "")
