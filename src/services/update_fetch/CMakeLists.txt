ot_module(SRCS https_client.c update_poll.c
          INCLUDE_DIRS include
          REQUIRES update
          PRIV_REQUIRES activity events freertos lwip mbedtls platform settings storage version wireless)

# The roots the unit trusts, as one PEM text (https_client.c): GitHub's two (github_roots.pem). A test build
# (-DHTTPS_TEST_CA=ON) trusts the local OTA server's test roots in their place (tools/ota_server.py --make-certs),
# two of the same kinds, so a fetch on the bench holds what a user's does (ADR-008: with all four, 3.3 KB more); no
# other build does, and a test build reaches no GitHub, as the bench rules ask
option(HTTPS_TEST_CA "Test build only: trust the local OTA server's test roots in place of GitHub's" OFF)
set(HTTPS_CA_FILES ${CMAKE_CURRENT_LIST_DIR}/github_roots.pem)
if (HTTPS_TEST_CA)
    set(HTTPS_CA_FILES ${CMAKE_SOURCE_DIR}/logs/ota_server/certs/ecc_root.pem
                       ${CMAKE_SOURCE_DIR}/logs/ota_server/certs/rsa_root.pem)
endif()
set(https_ca_text "")
foreach(ca_file IN LISTS HTTPS_CA_FILES)
    if (NOT EXISTS ${ca_file})
        message(FATAL_ERROR "${ca_file} is missing: run tools/ota_server.py --make-certs --host <this PC's address>")
    endif()
    file(READ ${ca_file} ca_pem)
    string(APPEND https_ca_text "${ca_pem}")
endforeach()
string(REPLACE "\r" "" https_ca_text "${https_ca_text}")
string(REPLACE "\n" "\\n\"\n    \"" https_ca_text "${https_ca_text}")
file(CONFIGURE OUTPUT ${CMAKE_BINARY_DIR}/https/https_ca.h
     CONTENT "// Made by the update_fetch module's CMakeLists.txt from HTTPS_CA_FILES\nstatic const char https_ca_pem[] =\n    \"${https_ca_text}\";\n"
     @ONLY)
target_include_directories(ot_update_fetch PRIVATE ${CMAKE_BINARY_DIR}/https)
