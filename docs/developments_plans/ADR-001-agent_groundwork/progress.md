# ADR-001 progress

What was built and measured, step by step. The plan is in the [README](README.md).

## Step 1: the process

Built 2026-11-13.

- `AGENTS.md`, `requirements.txt`, and `venv/` and `logs/` in `.gitignore`.
- **The check:** a fresh agent was given `AGENTS.md` and nothing else, and asked five questions: how to get a Pico 2 W image from a clone, what proves a change, whether it may flash a unit now, what adding a field to a settings struct does, and whether it may merge a contributor's PR. All five answers were right.
- It named two gaps, both fixed in the file:
  - the file did not say where `app.uf2` ends up (`build\app.uf2`, `build_w\app.uf2`);
  - it was not clear that flashing with no hands is still under the bench rule (it is: the rules hold for every way of reaching a unit).

## Step 2: build here, and know the size

Built 2026-11-13. One part of its "done when" is left: the CI run that shows both tables.

**Was a toolchain already on this PC?** No. Searched: `PATH`, `.pico-sdk` under every user profile, Program Files, ProgramData, AppData and the VS Code folders. The VS Code Pico extension (0.23.0) is installed but had fetched nothing. The old build folders' caches named `C:\Users\Ran Bao\.pico-sdk`, a profile that is not on this PC.

**`tools/setup_toolchain.ps1`**

| Part | Fetched | Reports |
|---|---|---|
| Arm GNU toolchain | 298 MB | `arm-none-eabi-gcc` 14.2.1 20241119 (14.2.Rel1) |
| CMake | 44 MB | 3.31.5 |
| Ninja | 0.3 MB | 1.12.1 |
| picotool | 1.5 MB | v2.1.1 |
| pioasm (pico-sdk-tools) | 0.8 MB | present with its CMake files |

- 1.4 GB on disk in `%USERPROFILE%\.pico-sdk`. `configure_env.ps1` is unchanged.
- A second run fetched nothing and left nothing in the temp folder.
- **The first run failed** before it installed anything. The script assumed the Arm archive holds one top folder; it holds its files directly. The cause was an assumption written down per archive, so the fix was to stop assuming: the script now looks for the file it needs inside what it unpacked. It also removes its temp folder when it fails, which the first version did not (about 1.3 GB was left behind, and removed by hand).

**The old build folders.** `build/` and `build_w/` held the v1.20 build from another PC, and their CMake caches named that PC's compiler. They were moved aside, not deleted: `build_v1.20_reference/` and `build_w_v1.20_reference/` (both ignored by git). `build/.placeholder`, which is tracked, stayed.

**The builds,** from empty build folders, with the repository's own scripts:

| | `build_pico_2w.ps1` | `build_pico_w.ps1` |
|---|---|---|
| Result | 368 of 368 steps | 368 of 368 steps |
| Time | 8 s | 6 s |
| Version in the image | 1.20.14-dirty at 7c70131 (step 1's commit; step 2's files were not yet committed) | the same |
| Warnings | 4 | 4 |

- All the warnings are in u8g2's MUI (`library/u8g2/csrc/mui.c`, `mui_u8g2.c`): an unused function and an unused variable, each printed twice. None is from `src/`.

**The sizes** (`tools/size_report.py`):

| | Pico 2 W | Pico W |
|---|---|---|
| Flash | 663.4 KB of 4,096 | 688.6 KB of 2,048 |
| RAM at link time | 195.4 KB of 512 | 194.8 KB of 256 |
| The report's total against `app.bin` | +0.3% | +0.3% |

| Flash by part | Pico 2 W | Pico W |
|---|---|---|
| The radio: cyw43's driver and its firmware | 249.9 KB | 251.6 KB |
| lwIP | 93.4 KB | 105.4 KB |
| Web pages | 89.7 KB | 89.8 KB |
| C and C++ runtime | 68.9 KB | 71.6 KB |
| Pico SDK | 50.3 KB | 52.9 KB |
| Application | 49.2 KB | 51.6 KB |
| FreeRTOS | 29.5 KB | 28.4 KB |
| u8g2 and MUI | 27.9 KB | 31.5 KB |
| HTTP server | 3.5 KB | 4.0 KB |
| Trinamic | 1.1 KB | 1.6 KB |

- The image built here and v1.20's image from the other PC give the same table to 0.1 KB: main has no code change since v1.20.
- **A fault found in the report, and fixed.** Its first version told the parts apart by words anywhere in an object's path. On this PC the compiler lives in a folder named `.pico-sdk`, so the C runtime (70 KB) was counted as the Pico SDK; the same loose matching had put `mui_menu.c` under u8g2 and `freertos_hooks.c` under FreeRTOS. A part is now told by where its source sits in the repository, and anything from outside the tree is the compiler's runtime. The table no longer depends on where anything is installed.
- The numbers in the first drafts of ADR-005 and ADR-010 came from the loose version and were corrected (the application is 52 KB on Pico W, not 48; u8g2 and MUI 32 KB, not 35).

**Left in this step:** the CI run of this branch that shows both tables in its job summary. CI runs on pushes to `main`, on PRs and when started by hand, so it needs the branch pushed, which waits for the owner's word. CI builds with Make where this PC builds with Ninja; the report has not yet been seen on a map made that way.

## Steps 3 to 6

Not started. They need the owner's word that the bench is ready.
