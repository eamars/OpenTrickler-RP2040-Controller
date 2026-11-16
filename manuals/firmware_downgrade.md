# Going back to an older firmware

An update over Wi-Fi only ever installs a newer version. To go back to an older one, the unit is erased and the older version is installed by USB. **The settings are erased too:** an older firmware may not read the settings a newer one stored, so it starts from its defaults, and you set them again.

You need the files from two releases:
- from the release the unit runs now (or any newer one), the erase for your board: `pico_w_erase.uf2` or `pico2_w_erase.uf2`;
- from the older release, its file for your board: `pico_w_full_install.uf2` or `pico2_w_full_install.uf2`, or, for v1.20 and older, `app.uf2` from its zip.

The board's name (Pico W or Pico 2 W) is printed on the Pico.

1. If you want a record of your settings, open the unit's web page and press **Export Config** on its **System Control** page. An older firmware may not take them back, so keep it as a record of what you had.
2. At the unit, open **Settings → EEPROM → Erase EEPROM** and confirm. The unit restarts with its settings erased.
3. Disconnect the controller board from the 12/24V power supply. Hold the BOOTSEL button on the Pico, connect it to your computer by USB, and release the button once the Pico shows as a drive ([Firmware Update via USB](firmware_update_via_usb.md), steps 4 to 7).
4. Drag and drop the **erase** file onto the drive. The Pico disconnects, then shows as a drive again by itself: there is no firmware left on it to start.
5. Drag and drop the **older release's file** onto the drive. The Pico disconnects: the older firmware is installed.
6. Disconnect the USB cable and connect the 12/24V power supply again. With its settings erased, the unit starts as its own Wi-Fi access point: join it and set the unit up again, Wi-Fi first ([Connect to Wireless](connect_to_wireless.md)).
