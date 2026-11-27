# Firmware Update via USB

This is how firmware is installed by cable: the first time, the move from an older version, and to bring back a unit that does not start. Your settings are kept: they are stored on the controller board, not on the Pico. Going back from version 2 to version 1 is not supported: version 1 resets the settings it cannot read.

> [!TIP]
> Some USB cables are charge-only and cannot transfer data. If your Raspberry Pi Pico does not appear as a drive on your computer after the steps below, try a cable that supports data transfer.

1. Find out which Pico your controller board carries: the two take different files, and the wrong one does not start. The board's name is printed on the Pico: Pico W or Pico 2 W. If you cannot read it, follow steps 4 to 7 below without a file, until the Pico shows as a drive; the drive's name tells you. Then disconnect the USB cable and come back to step 2.
   - a drive named `RPI-RP2`: a Raspberry Pi Pico W;
   - a drive named `RP2350`: a Raspberry Pi Pico 2 W.
2. From the [releases page](https://github.com/eamars/OpenTrickler-RP2040-Controller/releases), download the file for your board. The release marked **Latest** is version 1; version 2 is in testing, as releases marked **Pre-release**.
   - Version 2: each file's name starts with its version, `OpenTrickler-2.0.3-` for example, and ends `pico_w_full_install.uf2` for the Pico W or `pico2_w_full_install.uf2` for the Pico 2 W. Take the full install, not the `update` files.
   - Version 1: `app_pico_w_1.21.uf2` for the Pico W, or `app_pico_2w_1.21.uf2` for the Pico 2 W.
3. If you want a copy of your settings, open the unit's web page: in version 2, **Unit › Backup › Export settings**; in version 1, **Export Config** on its **System Control** page. The install keeps your settings, but a copy does no harm, and version 2's **Restore from a file** reads a copy from either.
4. Disconnect the OpenTrickler controller board from the 12/24V power supply.
5. Connect the small end of your micro USB cable to the Raspberry Pi Pico.
   ![plug-in-pico](../resources/firmware_update/pico-top-plug.png)
6. Hold down the BOOTSEL button on the Raspberry Pi Pico.
   ![bootsel](../resources/firmware_update/bootsel.png)
7. Connect the other end to your desktop computer, laptop, or Raspberry Pi while still holding the BOOTSEL button.
   ![pico-top-plug](../resources/firmware_update/plug-in-pico.png)
8. Your file manager opens, with the Raspberry Pi Pico shown as a drive. Release the BOOTSEL button. Drag and drop the file you downloaded onto the drive. The Pico disconnects and the file manager closes: the firmware is installed.
   ![file_manager](../resources/firmware_update/file_manager.png)
9. Disconnect the USB cable and connect the 12/24V power supply again. The unit starts on the new firmware.
