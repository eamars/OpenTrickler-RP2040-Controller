# ADR-010, gate 1: LVGL and TLS measured on the unit, beside today's firmware. A measure build only
# (-DADR010_MEASURE=ON): nothing in this folder is built otherwise. See measure_lvgl.c and measure_tls.c

set(MEASURE_DIRECTORY ${CMAKE_SOURCE_DIR}/measure)
set(LVGL_DIRECTORY ${CMAKE_SOURCE_DIR}/library/lvgl)

# LVGL, configured by measure/lv_conf.h. Every source is compiled; what lv_conf.h switches off compiles to nothing
file(GLOB_RECURSE LVGL_SOURCES ${LVGL_DIRECTORY}/src/*.c)
add_library(lvgl STATIC ${LVGL_SOURCES})
target_include_directories(lvgl PUBLIC ${LVGL_DIRECTORY}/include ${LVGL_DIRECTORY} ${MEASURE_DIRECTORY})
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)

# The roots the unit trusts, as one PEM string: the local OTA server's test roots (tools/ota_server.py --make-certs)
# and GitHub's two (measure/github_roots.pem: USERTrust ECC for github.com, ISRG Root X1 for its asset host)
set(ADR010_MEASURE_CA
    "${CMAKE_SOURCE_DIR}/logs/ota_server/certs/ecc_root.pem;${CMAKE_SOURCE_DIR}/logs/ota_server/certs/rsa_root.pem;${MEASURE_DIRECTORY}/github_roots.pem"
    CACHE STRING "The PEM files the measure build trusts")
set(measure_ca_text "")
foreach(ca_file IN LISTS ADR010_MEASURE_CA)
    if (NOT EXISTS ${ca_file})
        message(FATAL_ERROR "${ca_file} is missing: run tools/ota_server.py --make-certs --host <this PC's address>")
    endif()
    file(READ ${ca_file} ca_pem)
    string(APPEND measure_ca_text "${ca_pem}")
endforeach()
string(REPLACE "\r" "" measure_ca_text "${measure_ca_text}")
string(REPLACE "\n" "\\n\"\n    \"" measure_ca_text "${measure_ca_text}")
file(WRITE ${CMAKE_BINARY_DIR}/measure/measure_ca.h
     "// Made by measure/measure.cmake from ADR010_MEASURE_CA\nstatic const char measure_ca_pem[] =\n    \"${measure_ca_text}\";\n")

# mbedTLS at -O2 in every build, as sha256.c is (CMakeLists.txt): at the Debug build's -O0 a handshake with the ECDSA
# chain took 148 s on the Pico 2 W, and mbedTLS's assembly multiply does not compile at -O0 (no registers left)
file(GLOB MBEDTLS_SOURCES ${PICO_SDK_PATH}/lib/mbedtls/library/*.c)
set_source_files_properties(${MBEDTLS_SOURCES} PROPERTIES COMPILE_OPTIONS "-O2")

target_sources(${TARGET_NAME} PRIVATE ${MEASURE_DIRECTORY}/measure_lvgl.c ${MEASURE_DIRECTORY}/measure_tls.c)
target_include_directories(${TARGET_NAME} PRIVATE ${MEASURE_DIRECTORY} ${CMAKE_BINARY_DIR}/measure)
target_compile_definitions(${TARGET_NAME} PRIVATE ADR010_MEASURE=1)
target_link_libraries(${TARGET_NAME} lvgl pico_mbedtls)
