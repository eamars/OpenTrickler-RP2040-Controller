# ADR-007: the same firmware on the ESP32-S3-Pico

Status: **Draft** 2026-11-13. Not approved. The pins below are from Waveshare's page and drawing; nothing has been checked on the board.

## What we want

The ESP32-S3-Pico, in the controller's Pico socket, runs the same application: the same menu, the same charge, the same web UI, the same settings.

## Owner's direction (2026-11-13)

- Support the ESP32 platform.
- A compatible dev kit is connected to this computer and should be pin to pin compatible: the Waveshare ESP32-S3-Pico (https://docs.waveshare.com/ESP32-S3-Pico).
- One implementation above the chip.

## Why here

It is the proof of ADR-006. A seam with one platform behind it is a guess.

## What we know (checked 2026-11-13, from Waveshare's page and pinout drawing)

**The board**
- ESP32-S3R2: two cores at 240 MHz, 512 KB RAM, 2 MB PSRAM in the package.
- 16 MB flash (W25Q128).
- USB-C with a CH343 serial chip and a CH334 hub chip. The page does not say which pins the serial chip serves, or whether the chip's own USB is behind the hub.
- A WS2812 LED on GPIO21, a BOOT button on GPIO0, a reset button.
- Power, ground, RUN and 3V3_EN are on the pins where a Pico has them.

**Our signals, by the pin they sit on**

| Signal | Pico | ESP32-S3 | | Signal | Pico | ESP32-S3 |
|---|---|---|---|---|---|---|
| Scale TX, RX | GP0, GP1 | 11, 12 | | Encoder A, B | GP15, GP14 | 40, 39 |
| Coarse DIR, STEP, EN | GP2, GP3, GP6 | 13, 14, 17 | | Display CS, SCK, MOSI | GP17, GP18, GP19 | 41, 1, 2 |
| TMC UART TX, RX | GP4, GP5 | 15, 16 | | Display A0, reset | GP20, GP21 | 4, 5 |
| Fine DIR, STEP, EN | GP7, GP8, GP9 | 18, **33, 34** | | Encoder button | GP22 | 6 |
| EEPROM SDA, SCL | GP10, GP11 | **35, 36** | | Servo 0, 1 | GP26, GP27 | 7, 8 |
| Reset button | GP12 | **37** | | LED PWM3 | GP28 | 9 |
| Neopixel | GP13 | 38 | | (Pico's ADC_VREF pin) | pin 35 | 10 |

- **GPIO33 to 37.** Waveshare's page says they are used by octal PSRAM. The fine motor's STEP and EN, the EEPROM and the reset button land on them. This module's PSRAM is the 2 MB kind, which normally uses other pins, and the drawing shows 33 to 37 as plain GPIO. Not confirmed.
- **The Pico's pin 35** is ADC_VREF. On this board it is GPIO10. What the controller board connects there is not known.
- No strapping pin (0, 3, 45, 46) is on the header.
- GPIO39 to 42 can serve as the chip's JTAG pins; the encoder and the display's CS sit on them. To be checked that they come up as plain pins.

**What ESP-IDF gives in place of the Pico SDK**
- The owner's notes on issue #83: step pulses by the RMT peripheral (ESP-IDF's stepper example), the LEDs by RMT (`led_strip`).
- Two application slots with rollback, for updates.
- The reference project builds on ESP-IDF 6.1, with LVGL, OTA, a console and Wi-Fi set-up already working there.

**From the code**
- Settings are in the EEPROM on the controller board. A controller that changes its module keeps its EEPROM, so the settings follow if both chips read the same layout (ADR-006).

**Handed on by ADR-006** (2026-11-27, Draft; ADR-006 builds none of this, the owner: "For ADR-006 I don't want to include ADR-007 changes")
- Every module is a folder whose build file makes one call, `ot_module`, with the arguments of ESP-IDF's `idf_component_register` and no others. Here the call gets its ESP-IDF branch, which passes them on, as LVGL's own build does; and an `idf.py` project whose component folders are ADR-006's layer folders. A module's name is its folder's, no two the same, as ESP-IDF needs.
- `platform/esp32s3/` behind ADR-006's interfaces, and `boards/esp32s3_pico/`: its pins, its modules, each module's optimisation, and ESP-IDF's own settings (sdkconfig) in place of `FreeRTOSConfig.h`, `lwipopts.h` and `mbedtls_config.h`.
- Stored enums are written as fixed-width types by ADR-006, so a record's bytes no longer hang on an enum's size (one byte with the Pico's compiler; four with ESP-IDF's, as far as is known, not checked). Whatever else the two compilers lay out apart is checked here: every record's size and offsets, by both.
- ADR-006's PC build is the modules' second build before this one.

## Ideas

- **The pins first.** Before any code: on the board, with a meter and one small test program, show that GPIO33 to 37 work as plain pins and what sits on pin 35. If 33 to 37 are not free, the port needs a different module or a changed controller board, and the owner decides.
- **An ESP-IDF build of the same tree**, with `platform/esp32/` behind ADR-006's interfaces:
  - step pulses by RMT; LEDs by RMT; servos by the LED PWM driver;
  - the two serial ports, SPI for the display, I2C for the EEPROM by ESP-IDF's drivers;
  - Wi-Fi and mDNS by ESP-IDF, the HTTP server ours (ADR-003) on its sockets;
  - updates by ESP-IDF's two slots behind ADR-004's interface;
  - the console and the remote control (ADR-001) on the board's USB serial port.
- **The same ESP-IDF version as the reference project**, so what was learned there applies.
- **The board's own LED** (GPIO21) for what the Pico W's on-board LED shows today.
- **USB recovery** is the chip's ROM loader, entered with the BOOT button. Shown, as on the Pico.
- **Its tier.** With 16 MB it can carry the extended modules (ADR-005).

## How the unit shows it works

On the ESP32-S3-Pico in the controller's socket:
- `menu_tour` gives the same forms as the Pico 2 W's baseline;
- `charge_cycle` passes;
- the owner's real throw: time and accuracy as on the Pico 2 W with the same profile;
- a controller set up with a Pico 2 W, then fitted with the ESP32, has its settings;
- an update over Wi-Fi, and a recovery by USB;
- ADR-008's web UI, the same pages, with ADR-008's checks: every page from a phone on the unit's access point with no internet, commissioning opened by the captive portal, the unit's report of what it carries (its larger flash may carry more) and only those pages shown (handed on by ADR-008, 2026-11-18: ADR-008 is proven on the Pico W and the Pico 2 W, as this board comes after it);
- the step pulses on a scope or logic analyser against the Pico 2 W's, at the lowest and the highest speed.

## Not in this ADR

- A larger or colour screen. The mini 12864 stays.
- Bluetooth.
- Anything the ESP32 could do that the Pico does not: the application is the same.

## To settle before approval

- The ESP-IDF version.
- Whether there is one controller board for both modules on the bench, or two.
