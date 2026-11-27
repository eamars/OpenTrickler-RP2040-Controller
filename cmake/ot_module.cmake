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
