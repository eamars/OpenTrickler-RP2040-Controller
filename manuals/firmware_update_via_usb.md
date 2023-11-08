# Firmware Update via USB

1. Download the latest debug version (uf2-debug) of OpenTrickler controller firmware at [![Auto Release Build](https://github.com/eamars/OpenTrickler-RP2040-Controller/actions/workflows/cmake.yml/badge.svg?branch=main)](https://github.com/eamars/OpenTrickler-RP2040-Controller/actions/workflows/cmake.yml?query=branch%3Amain+actor%3Aeamars), unzip the package and look for app.uf2. 
2. Disconnect the OpenTrickler controller board from the 12/24V power supply. 
3. Connect the small end of your micro USB cable to the Raspberry Pi Pico W
   ![plug-in-pico](../resources/firmware_update/pico-top-plug.png)
4. Hold down the BOOTSEL button on your Raspberry Pi Pico W
   ![bootsel](../resources/firmware_update/bootsel.png)
5. Connect the other end to your desktop computer, laptop, or Raspberry Pi. 
   ![pico-top-plug](../resources/firmware_update/plug-in-pico.png)
6. Your file manager should open up, with Raspberry Pi Pico being show as an externally connected drive. Drag and drop the firmware file you downloaded (app.uf2) into the file manager. Your Raspberry Pi Pico should disconnect and the file manager will close.
   ![file_manager](../resources/firmware_update/file_manager.png)
