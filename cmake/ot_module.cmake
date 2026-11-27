# The firmware's modules (ADR-006). A module is a folder named for it, src/<layer>/<module>/, whose build file makes
# one call:
#
#   ot_module(SRCS ... INCLUDE_DIRS ... PRIV_INCLUDE_DIRS ... REQUIRES ... PRIV_REQUIRES ...)
#
# The arguments are those of ESP-IDF's idf_component_register, and no others, so that ADR-007 gives this call an
# ESP-IDF branch that passes them on. This is the Pico SDK's branch: a module is a static library. The PC build has
# the call's second form, for the host tests (tests/host/CMakeLists.txt).
#   - INCLUDE_DIRS and REQUIRES are public: whoever requires the module gets them too.
#   - PRIV_INCLUDE_DIRS and PRIV_REQUIRES are the module's own.
#   - A module can include only the headers of what it names: the compiler refuses the rest. That is what makes the
#     layout hold, not the folders.
#   - A name in REQUIRES is another module's, or third-party code's by ESP-IDF's own name (freertos, lwip, mbedtls,
#     lvgl), made with ot_third_party() below.
#   - A module with no SRCS is its headers only.
#   - The module's target is ot_<name>. What the five arguments cannot say (a file the build makes, say) is said with
#     CMake's own commands on that target, after the call.
#
# The image links every module whole (ot_link_modules): a module's files take part in the link as they did on the
# link line. A linker takes a file out of a static library only for a name still undefined, so a file that only
# takes the place of a weak definition, as fault_handler.c does for the SDK's isr_hardfault, would be left out with
# no error (tried in gate 1: the image linked, with the hard fault's handler and the motors' stop gone).

# What every module compiles with: the image's own definitions and options, whatever the module requires. One
# view of the SDK's and the RTOS's settings for the whole firmware, as when everything was one program. A module's
# include paths are not here: those come only from what it requires.
function(ot_image_settings image)
    add_library(ot_image_settings INTERFACE)
    target_compile_definitions(ot_image_settings INTERFACE $<TARGET_PROPERTY:${image},COMPILE_DEFINITIONS>)
    target_compile_options(ot_image_settings INTERFACE $<TARGET_PROPERTY:${image},COMPILE_OPTIONS>)
endfunction()

# Third-party code as a module sees it: the include paths of the libraries named, and of all they need, and no
# source. The Pico SDK's and FreeRTOS's libraries are INTERFACE libraries that carry their sources, so a module that
# linked one would compile a copy of it; the image links the real ones, once.
#
# The SDK's libraries answer by who asks: what a target has chosen for itself (pico_enable_stdio_rtt() and the like)
# changes what they give it, and a module is not the image. So nothing the modules see is chosen on the image alone:
# the root CMakeLists.txt chooses the console for every target (PICO_STDIO_RTT), and a module's definitions are the
# image's (above).
#
# The image's own include paths cannot be handed out in parts in place of this: CMake works a property of a target
# out in full only the first time it meets it within one evaluation, so a second part of the same list comes back
# without the SDK's paths (tried in gate 1).
#   ot_third_party(<name> <library>...)
function(ot_third_party name)
    add_library(ot_${name} INTERFACE)
    add_library(ot::${name} ALIAS ot_${name})
    foreach(library IN LISTS ARGN)
        target_include_directories(ot_${name} SYSTEM INTERFACE
                                   $<TARGET_PROPERTY:${library},INTERFACE_INCLUDE_DIRECTORIES>)
    endforeach()
endfunction()

function(ot_module)
    cmake_parse_arguments(PARSE_ARGV 0 arg "" "" "SRCS;INCLUDE_DIRS;PRIV_INCLUDE_DIRS;REQUIRES;PRIV_REQUIRES")
    if (arg_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "ot_module: not an argument of idf_component_register: ${arg_UNPARSED_ARGUMENTS}")
    endif()
    # A module's name is its folder's, and no two are the same
    get_filename_component(name "${CMAKE_CURRENT_LIST_DIR}" NAME)
    if (TARGET ot_${name})
        message(FATAL_ERROR "ot_module: two modules are named ${name}")
    endif()
    list(TRANSFORM arg_REQUIRES PREPEND ot::)
    list(TRANSFORM arg_PRIV_REQUIRES PREPEND ot::)

    if (arg_SRCS)
        add_library(ot_${name} STATIC ${arg_SRCS})
        target_include_directories(ot_${name} PUBLIC ${arg_INCLUDE_DIRS} PRIVATE ${arg_PRIV_INCLUDE_DIRS})
        target_link_libraries(ot_${name} PUBLIC ${arg_REQUIRES} PRIVATE ${arg_PRIV_REQUIRES} ot_image_settings)
        # Its optimisation, where its board names one (ADR-006, gate 9: OT_MODULE_OPTIONS_<name> in board.cmake),
        # after the image's own, so it is the one that holds
        if (DEFINED OT_MODULE_OPTIONS_${name})
            target_compile_options(ot_${name} PRIVATE ${OT_MODULE_OPTIONS_${name}})
        endif()
        set_property(GLOBAL APPEND PROPERTY OT_MODULES ot_${name})
    else()
        if (arg_PRIV_INCLUDE_DIRS OR arg_PRIV_REQUIRES)
            message(FATAL_ERROR "ot_module: ${name} has no sources, so nothing of its own to keep private")
        endif()
        add_library(ot_${name} INTERFACE)
        target_include_directories(ot_${name} INTERFACE ${arg_INCLUDE_DIRS})
        target_link_libraries(ot_${name} INTERFACE ${arg_REQUIRES})
    endif()
    add_library(ot::${name} ALIAS ot_${name})
endfunction()

# CMake defines the WHOLE_ARCHIVE link feature for the systems it knows; the Pico SDK's toolchain file names its
# own (CMAKE_SYSTEM_NAME PICO), for which CMake defines none. Arm's linker is GNU ld: these are CMake's own lines
# for it (Modules/Platform/Linker/GNU.cmake)
if (NOT CMAKE_LINK_LIBRARY_USING_WHOLE_ARCHIVE_SUPPORTED)
    set(CMAKE_LINK_LIBRARY_USING_WHOLE_ARCHIVE "LINKER:--push-state,--whole-archive" "<LINK_ITEM>" "LINKER:--pop-state")
    set(CMAKE_LINK_LIBRARY_USING_WHOLE_ARCHIVE_SUPPORTED TRUE)
    set(CMAKE_LINK_LIBRARY_WHOLE_ARCHIVE_ATTRIBUTES LIBRARY_TYPE=STATIC DEDUPLICATION=YES OVERRIDE=DEFAULT)
endif()

# Every module linked into the image, whole
function(ot_link_modules image)
    get_property(modules GLOBAL PROPERTY OT_MODULES)
    list(JOIN modules "," modules)
    # No PRIVATE: the image's other libraries are given by the plain signature, and CMake takes one or the other
    target_link_libraries(${image} "$<LINK_LIBRARY:WHOLE_ARCHIVE,${modules}>")
endfunction()

# A part a build may leave out (ADR-006, gate 9): a module of the board's list that is also a CMake option, on by
# default, and named in the unit's `features`, present or not. Declared once, before ot_board_resolve():
#   ot_feature(<name> <module's folder under src/> "<what the unit's features says of it>")
# Its option is FEATURE_<NAME>. Only the start-up's wiring (app/) reads FEATURE_<NAME>: the rest of the firmware is
# handed the part's calls or not (tools/module_graph.py holds this).
function(ot_feature name module about)
    string(TOUPPER "${name}" upper)
    option(FEATURE_${upper} "Carry ${name}: ${about}" ON)
    set_property(GLOBAL APPEND PROPERTY OT_FEATURES "${name}")
    set_property(GLOBAL PROPERTY OT_FEATURE_MODULE_${name} "${module}")
    set_property(GLOBAL PROPERTY OT_FEATURE_ABOUT_${name} "${about}")
endfunction()

# The board's list, as this build carries it: each part a build may leave out taken out where its option is off or
# its board does not list it. Sets, in the caller's scope, OT_BOARD_MODULES as carried and OT_FEATURES_CARRIED (the
# parts' modules carried, which the start-up's wiring requires); gives the image FEATURE_<NAME> 1 or 0; prints the
# `Features:` line; and makes the table the unit's `features` is made from, in this build's folder (ot::features,
# ot_features.h): a build of each board in its own folder, each with its own.
function(ot_board_resolve image)
    get_property(features GLOBAL PROPERTY OT_FEATURES)
    set(modules ${OT_BOARD_MODULES})
    set(carried "")
    set(said "")
    set(table "")
    foreach(name IN LISTS features)
        string(TOUPPER "${name}" upper)
        get_property(module GLOBAL PROPERTY OT_FEATURE_MODULE_${name})
        get_property(about GLOBAL PROPERTY OT_FEATURE_ABOUT_${name})
        if (FEATURE_${upper} AND module IN_LIST modules)
            set(present 1)
            list(APPEND carried ${name})
        else()
            set(present 0)
            list(REMOVE_ITEM modules ${module})
        endif()
        target_compile_definitions(${image} PRIVATE FEATURE_${upper}=${present})
        list(APPEND said "${name}=${present}")
        string(REPLACE "\\" "\\\\" about "${about}")
        string(REPLACE "\"" "\\\"" about "${about}")
        if (present)
            string(APPEND table "    {\"${name}\", true, \"${about}\"},\n")
        else()
            string(APPEND table "    {\"${name}\", false, \"${about}\"},\n")
        endif()
    endforeach()
    list(JOIN said " " said)
    message(STATUS "Features: ${said}")
    file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/generated/ot_features.h" CONTENT
"// What this build carries (ADR-006, gate 9): made by CMake from the board's list (cmake/ot_module.cmake), every
// part a build may leave out, carried or not, the last entry's name NULL. Read by the API's index (`features`)
#ifndef OT_FEATURES_H_
#define OT_FEATURES_H_
#include <stdbool.h>
#include <stddef.h>
typedef struct {
    const char *name;
    bool present;
    const char *about;
} ot_feature_t;
static const ot_feature_t ot_features[] = {
${table}    {NULL, false, NULL},
};
#endif
")
    add_library(ot_features INTERFACE)
    add_library(ot::features ALIAS ot_features)
    target_include_directories(ot_features INTERFACE "${CMAKE_BINARY_DIR}/generated")
    set(OT_BOARD_MODULES ${modules} PARENT_SCOPE)
    set(OT_FEATURES_CARRIED ${carried} PARENT_SCOPE)
endfunction()

# A place only one module of a build can fill (ADR-006, gate 9): the chip's side of the platform, the screen's view.
# A module says which it fills, after its ot_module() call:
#   ot_fills(<place>)
# and once every module is in, ot_check_places(<place>...) refuses a board's list with none for a place, or two.
function(ot_fills place)
    get_filename_component(name "${CMAKE_CURRENT_LIST_DIR}" NAME)
    set_property(GLOBAL APPEND PROPERTY OT_PLACE_${place} ${name})
endfunction()

function(ot_check_places)
    foreach(place IN LISTS ARGN)
        get_property(fillers GLOBAL PROPERTY OT_PLACE_${place})
        list(LENGTH fillers count)
        if (count EQUAL 0)
            message(FATAL_ERROR "The board's list has no module for the ${place}: a build has one "
                                "(src/boards/${PICO_BOARD}/board.cmake)")
        elseif (count GREATER 1)
            list(JOIN fillers ", " fillers)
            message(FATAL_ERROR "The board's list has ${count} modules for the ${place} (${fillers}): a build has "
                                "one (src/boards/${PICO_BOARD}/board.cmake)")
        endif()
    endforeach()
endfunction()
