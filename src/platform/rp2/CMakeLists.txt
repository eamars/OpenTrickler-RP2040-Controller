ot_module(SRCS chip_rp2.c dhcpserver.c fault_handler.c freertos_hooks.c picobin_image.c rtos_port_rp2.c step_pulses.c update_slots_rp2.c
               update_slots_rp2040.c update_slots_rp2350.c
          INCLUDE_DIRS include
          REQUIRES lwip rp2_sdk
          PRIV_REQUIRES board freertos mbedtls platform)

# The PIO programs' headers, made by pioasm into this module's build folder. The LEDs' module includes one of them
# until gate 7 puts the strip behind the platform's interface, so the folder is given to whoever requires rp2
pico_generate_pio_header(ot_rp2 ${CMAKE_CURRENT_LIST_DIR}/stepper.pio)
pico_generate_pio_header(ot_rp2 ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)
target_include_directories(ot_rp2 PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
