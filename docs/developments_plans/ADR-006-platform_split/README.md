# ADR-006: the application apart from the platform

Status: **Draft** 2026-11-13. Not approved. Captures what ties the application to the Pico today and how to part them.

## What we want

- No Pico SDK, lwIP or cyw43 name outside a platform folder.
- A layout by role, so a file's place says what it is.
- The same behaviour as before, shown on the unit after every step.

## Owner's direction (2026-11-13)

- Abstract the application layer from the target, to support the ESP32 platform.
- One implementation above the chip.
- Some parts need a rewrite, not a move. The HTTP stack is one (ADR-003). The owner has more to name; each either joins this ADR as a step or gets its own.

## Why here

After the PRs are decided (ADR-002), so nothing waiting to be merged is moved under its author. After ADR-003, so the network's largest tie, the REST handlers, is already cut. Before ADR-007, which is its proof.

## What we know (checked 2026-11-13)

- `src/` is one flat folder: 33 `.c` files, 4 `.cpp`, about 12,750 lines with the headers. `targets/` holds one pin map, and `configuration.h` includes it by name.
- 20 of the 37 source files include `pico/` or `hardware/` headers: all eight scale drivers, `motors.c`, `servo_gate.c`, `eeprom.c`, `cat24c256_eeprom.c`, `common.c`, `mui_menu.c`, `mini_12864_module.cpp`, `system_control.c`, `wireless.c`, `access_point_mode.c`, `dhcpserver.c`, `app.c`.
- `common.h` includes `hardware/pio.h` and FreeRTOS, and almost everything includes `common.h`.
- Three places reach past the SDK into registers: the TMC2209's one-wire UART switches the receiver off while it sends (`motors.c`), the unit's id is made from the ring oscillator's random bit (`eeprom.c`), and the servo writes a PWM slice's register (`servo_gate.c`).
- Two PIO programs: `stepper.pio` makes the step pulses, `ws2812.pio` drives the LEDs.
- `gpio_irq_handler.h` is a C++ class for the encoder's and buttons' interrupts.
- Tasks are made by 11 `xTaskCreate` calls, with stack sizes in words. FreeRTOS runs on both cores.
- Wi-Fi is cyw43 with lwIP. The access point's DHCP and DNS servers are files from the Pico examples.
- Stored settings are the C structs as compiled. They hold enums (`decimal_places_t`, `scale_driver_t`, `display_rotation_t`, and cyw43's own `cyw43_auth_t` in the Wi-Fi settings) and bools. Two compilers need not lay these out alike.
- The scale drivers each poll `uart_is_readable(SCALE_UART)` in their own loop.
- The simulated scale reads the motors' speeds straight out of `motors.c`'s structs (`extern motor_config_t`), and charge mode's target the same way.
- The stored charge-mode struct gained three fields in the middle with PR #108, which changes its layout.

## Ideas

**A layout by role**, after the reference project's:

| Folder | What belongs there |
|---|---|
| `app/` | Start-up order and wiring |
| `board/` | One header per board: pins, buses, what is fitted |
| `platform/rp2/` | Everything that includes `pico/`, `hardware/` or cyw43. `platform/esp32/` comes in ADR-007 |
| `services/` | Scale and its drivers, motors, servo gate, LEDs, profiles, storage, charge, cleanup, Wi-Fi |
| `ui/` | The menu and the screens |
| `web/` | The HTTP server, the REST handlers, the pages |
| `tools/` | The remote control |
| `common/` | Helpers with nothing of the above in them |

**Small interfaces, one at a time**, each a header in `platform/` with one implementation per chip:
- storage: read and write a block;
- a serial port: open, set its format, read, write, and "receiver off" for the TMC's one-wire link;
- step pulses: a rate for a pin, and stop;
- an LED strip: write colours;
- a servo: a pulse width;
- inputs: a pin's edges as events (the encoder, the buttons);
- the display's bus: write bytes, and the three control pins;
- the network: start as access point or station, its state and address. Sockets are already the same on both;
- the system: restart, enter the USB loader, the unit's id, a random number, the time;
- a task: one call that takes a stack size in bytes, since the two FreeRTOS ports count it differently.

**Steps**, each proven on the unit before the next:
1. Move files into the folders. No line of code changes.
2. One interface at a time, in the order above.
3. Stored settings in a layout that does not depend on the compiler, with the old layout read once and written back in the new one, so no unit loses its settings.
4. The build: one list of sources, used by the Pico SDK's build and, in ADR-007, by ESP-IDF's.

**No second platform is written here.** ADR-007 is what shows the interfaces are the right ones; if it finds one that is wrong, it is changed there.

## How the unit shows it works

After every step, on the Pico 2 W controller:
- `menu_tour` equal to the baseline, snap for snap;
- `charge_cycle` passes;
- the size report within a few KB of before.

At the end:
- the owner's real throw;
- a search finds no `pico/`, `hardware/`, `cyw43` or `lwip/` include outside `platform/` and `board/`;
- a unit updated from the release before keeps its settings.

## Not in this ADR

- The screens. Services stop drawing in ADR-010.
- The controller (ADR-009).
- Any change a user can see.

## To settle before approval

- The other rewrites the owner wants, and whether each is a step here or an ADR of its own.
- Whether C++ stays where it is (`charge_mode.cpp`, `cleanup_mode.cpp`, `mini_12864_module.cpp`, `FloatRingBuffer.cpp`).
