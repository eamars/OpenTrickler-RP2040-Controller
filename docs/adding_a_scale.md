# Adding support for a scale

This guide is for anyone who wants the OpenTrickler to read a scale it does not support yet. It takes you from a scale on your bench to a pull request: first whether you need new code at all, then how to learn and record what the scale sends, then the code, the tests and the proof on a unit.

The scales supported today are listed in the [README](../README.md#supported-scales).

## 1. First, try the Generic make

The firmware has a make called **Generic**. It reads any line that ends in LF and takes the first number on it as the weight. Many scales that stream their weight need nothing more. For example, the Kern lines in [issue #15](https://github.com/eamars/OpenTrickler-RP2040-Controller/issues/15) (`        0.01 gn \r\n`) read correctly with it.

Choose it on the unit (**Settings › Scale**, or the web page's **Settings › Scale**), set the baud rate and format to match the scale, and put something on the pan. If the reading follows the pan, you may be done. Send a pull request that adds a row for your scale to the README's table, marked as read with the Generic make.

You need a make of your own when any of these is true:
- **The scale must be asked for each reading.** It sends nothing until it receives a command (the G&G does this, and so do many Mettler Toledo, Ohaus and Sartorius balances unless they are set to stream).
- **You want the unit to zero the scale.** Generic sends no zero command.
- **Its line holds other numbers before the weight,** such as a counter or a date, which Generic would read as the weight.
- **Its line must be checked before it is trusted:** a status that marks an error or an overload, or a fixed length that shows the line arrived whole.
- **Its frames are not lines:** they start with a header byte and run for a fixed length (the JM Science does this).

## 2. Learn the scale's protocol

The scale's manual has a chapter on its RS-232 or "data interface". Find in it:
- **The serial settings:** baud rate, data bits, parity and stop bits. The unit offers 4800, 9600 and 19200 baud, and 8N1 or 7N1 today (see step 5 for anything else).
- **How the weight is sent:** streamed continuously, or only in answer to a command (and which command).
- **The layout of one line:** its length, where the sign, the weight and the unit sit, what marks a stable or an unstable reading, and what an error or overload line looks like.
- **The zero or tare command,** and what the scale answers to it.

Most balances let you choose these settings from their own menu, as the [A&D](../manuals/Scale-Settings) and [G&G](../manuals/configure_g&g_scales.md) guides show. Set the scale to send **unstable readings too**: the charge needs a reading while powder is still falling, and a scale that only sends stable readings is too slow to trickle with.

## 3. Record what the scale really sends

A manual can be wrong or describe another model, so record real lines before you write any code. Connect the scale to a PC with a USB to RS-232 adapter and run this from the repository's venv (pyserial is already installed). Set the port and the scale's serial settings:

```python
import serial, time

REQUEST = None          # for a scale that must be asked, its request, e.g. b"SI\r\n"

port = serial.Serial("COM5", 9600, bytesize=8, parity="N", stopbits=1, timeout=0.5)
end = time.time() + 30
while time.time() < end:
    if REQUEST:
        port.write(REQUEST)
    line = port.readline()
    if line:
        print(f"{time.time():.3f} {line!r}")
```

While it runs, do everything the unit will meet: an empty pan, a weight put on and taken off, a negative reading, a change of unit if the scale allows it, its zero or tare key pressed, and an overload if you can make one safely. Keep the output and attach it to your issue or pull request; the framing test in step 6 is built from it.

If you have a unit with a debug probe, the unit's own console shows the same thing with `scale tap on`: every frame it receives, as bytes, and the weight it read.

## 4. Write the make

A make is one C file in `src/drivers/scale_protocols/`. It holds no task and no state: one table entry that says how frames are found, and a parser that turns one frame into a weight. The scale's single reader task does the rest, for every make. The table entry is `scale_make_t` in [`scale_make.h`](../src/drivers/scale_protocols/include/scale_make.h):

| Field | What it says |
|---|---|
| `framing` | `SCALE_FRAME_AT_LF` (a line ended by LF), `SCALE_FRAME_AT_CR_OR_LF`, or `SCALE_FRAME_AT_HEADER` (a frame from its `header` byte for its `length`) |
| `length` | The frame's exact length in bytes, line ending included. A line of any other length is refused, never read. `0` takes a line of any length up to 32 bytes |
| `header` | For `SCALE_FRAME_AT_HEADER`: the byte every frame starts with |
| `parse` | Your parser: the frame's bytes in, the weight out, or `NAN` for a frame that holds no weight |
| `unit_at`, `unit_len` | Where the weight's unit sits in the frame, if it carries one; `0` for none |
| `zero` | The command that zeroes the scale, line ending included; `NULL` if there is none |
| `request`, `request_ms` | For a scale that must be asked: the request, sent every `request_ms`. `NULL` for a scale that streams |

Rules for the parser, each learned from a fault:
- **Return `NAN` for anything that is not a weight:** an error line, an overload, an answer to a command, a line cut short. Never return 0 for "no number": 0 reads as an empty pan. Each `NAN` is counted (`bad_frames`) and publishes no weight.
- **Accept unstable readings.** The charge reads the weight while powder falls.
- **Check the bytes the layout fixes** (a header, a separator, the line ending) before reading the number, so a line from somewhere else does not read as a weight.
- **Read only the weight's field,** so a number elsewhere on the line is never taken for it.

A scale that answers its commands (a request, a zero) sends those answers on the same line as its weights. They reach your parser like any frame and should return `NAN`; they show in `bad_frames`, which is expected for such a scale.

## 5. Wire it in

1. **Its number.** Add the make to `scale_driver_t` in [`scale.h`](../src/services/scale/include/scale.h) with the next free value. Today that is **10**; 100 is the simulated scale. This number is stored in each unit's EEPROM, so a value is never renumbered or reused: check that no release used it (`git show <tag>:src/scale.h` for v1 tags), as [AGENTS.md](../AGENTS.md) explains. `tests/release_records.py` checks this in CI.
2. **Its file.** Add the `.c` file to `SRCS` in `src/drivers/scale_protocols/CMakeLists.txt`, and its `extern const scale_make_t ...;` line to `scale_make.h`.
3. **Register it.** In [`src/app/app/app.c`](../src/app/app/app.c), next to the others: `scale_register_make(SCALE_DRIVER_<NAME>, &<name>_scale_make);`.
4. **Name it.** In [`scale.c`](../src/services/scale/scale.c), add a row to `scale_driver_choices` (the name the API uses, in lower case, and the label people see) and a case to `get_scale_driver_string`. The screen's menu, the web page and the API's schema all take the new make from that table; nothing else names it.
5. **Its unit, if its frames carry one.** Today the unit is reported only for the A&D: `scale_get_unit` in `scale.c` checks for that make. A new make that fills `unit_at` and `unit_len` needs that check changed to ask whether the running make carries a unit.
6. **Serial settings the unit does not offer yet** (another baud rate, 7E1): append a value to `scale_baudrate_t` or `scale_uart_format_t` (never insert: these are stored too), its case in `get_scale_baudrate` or `set_scale_uart_format`, and its row in `scale_baud_choices` or `scale_format_choices`. The serial port itself already takes even and odd parity.

## 6. Test it on a PC

Add a case to [`tests/host/scale_framing_test.c`](../tests/host/scale_framing_test.c) with lines from your recording: whole frames read as their weights, and the lines that must not be weights (an error, a reply to the zero, a line cut short or run together with the next) read as `NAN`. The existing cases show the form. Then run:

```powershell
venv\Scripts\python tests\host_test.py
```

It builds the scale's code for the PC and ends `VERDICT PASS`. CI runs it on every build.

## 7. Prove it on a unit

Build and flash (see the [README](../README.md#building-from-source)), choose the new make under **Settings › Scale** with the scale's serial settings, and check:
- **The reading follows the pan,** on the unit's Scale page and on the web page's set-up (its scale step shows the live weight).
- **Zero works** from the unit's Scale page, if the make has a zero command.
- **Nothing is counted bad while the scale is quiet:** with a debug probe, `stats` gives `bad_frames`; with the tap on, every frame shows its weight. With a probe, `rc_run.py scale_frames` sums this up.
- **A charge runs:** a short charge with the hopper empty first, then with powder. Then run the calibration (**Settings › Calibration**), which measures the scale's lag; a new scale needs its own.

Then update the README's table of scales (its baud rate and format, and whether it zeroes), and, if the scale needs its own menu set up first, a guide in `manuals/` like the [G&G's](../manuals/configure_g&g_scales.md).

## A worked example: Mettler Toledo (MT-SICS)

[Issue #13](https://github.com/eamars/OpenTrickler-RP2040-Controller/issues/13) asks for a Mettler Toledo balance. Mettler Toledo balances speak MT-SICS. This is how its protocol maps onto a make, from the [MT-SICS reference manual](https://mt.com/content/dam/non-indexed/po/labtec/12_Miscellaneous/RM-MT-SICS-LA-en.pdf). **It is a sketch and has not been run against a real balance;** record one first (step 3).

What the manual says:
- **`SI`** asks for the current weight, stable or not. The answer is `S`, a space, a status (`S` stable, `D` dynamic), a space, the weight right-aligned in 10 characters, a space, the unit (1 to 5 characters), then CR LF: `S D     129.07 g`.
- The same command answers `S I` when the balance is busy (taring, for example), `S +` on overload and `S -` on underload.
- **`SIR`** sends the weight repeatedly until another command (`@`, `S`, `SI` and others) cancels it. A make sends no one-off command at its start, so it asks with `SI` at an interval instead.
- **`Z`** zeroes the balance. It answers `Z A` when done, `Z I` when it cannot (for example, it never became stable).
- Commands and answers end in CR LF. The serial settings are the balance's own; the manual's example of a host interface is 9600 baud, 8N1.

So its entry asks with `SI`, zeroes with `Z`, and takes a line of any length, because the unit's field varies:

```c
static float mt_sics_parse(const char *frame, size_t len) {
    // "S S     129.07 g\r\n": its id, a space, its status, a space, the weight right-aligned in 10, a space, the unit
    if (len < 16 || frame[0] != 'S' || frame[1] != ' ' || frame[3] != ' ') {
        return NAN;                         // another reply ("Z A", "ES"), or a line cut short
    }
    if (frame[2] != 'S' && frame[2] != 'D') {
        return NAN;                         // "S I" busy, "S +" overload, "S -" underload
    }
    char *end;
    float weight = strtof(frame + 4, &end);
    if (end == frame + 4 || *end != ' ') {
        return NAN;                         // no number, or not followed by the unit's space
    }
    return weight;
}


const scale_make_t mt_sics_scale_make = {
    .framing = SCALE_FRAME_AT_LF,
    .parse = mt_sics_parse,
    .zero = "Z\r\n",
    .request = "SI\r\n",
    .request_ms = 50,
};
```

The parser was compiled and run against the manual's examples: `S D     129.07 g` reads 129.07, `S S      -0.02 g` reads -0.02, and `S I`, `S +`, `Z A`, `ES` and a line with no unit read as no weight.

Two things only a real balance can settle:
- **The request interval.** A request and its answer take some 20 ms at 9600 baud, and the balance has its own update rate. The charge pauses when no reading has come for 300 ms, and the faster the readings, the closer it stops. Measure how often an answer comes, and choose `request_ms` from that.
- **The answer to `Z`** arrives between weights and is counted as a bad frame, as expected. Check that the balance does not skip the weight requests while it zeroes, or the charge pauses meanwhile.

## Other protocols you may meet

These are starting points from the makers' manuals, not tested here. Check your own model's manual, as the command letters differ between families.
- **Ohaus:** `IP` prints the weight at once, stable or not, and `P` prints the displayed weight; `CP` starts a continuous print; `Z` zeroes and `T` tares. The line layout differs by model.
- **Sartorius (SBI):** `ESC P` asks for a print and `ESC T` tares. Lines are 16 or 22 characters, set on the balance. The unit's existing Sartorius make reads a streaming balance and has no zero command yet.
- **A&D:** for another A&D balance, try the A&D FX-i make first. It reads the FX-i's standard format (`ST,+00012.34 GN`, 17 bytes with CR LF) and zeroes with `Z`.
