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

Built 2026-11-13, CI run included.

**Was a toolchain already on this PC?** No. Searched: `PATH`, `.pico-sdk` under every user profile, Program Files, ProgramData, AppData and the VS Code folders. The VS Code Pico extension (0.23.0) is installed but had fetched nothing. The old build folders' caches named `C:\Users\Ran Bao\.pico-sdk`, a profile that is not on this PC.

**`setup_toolchain.ps1`** (written as `tools/setup_toolchain.ps1`; moved to the root later, see "The branch was cut from an old main")

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

**The sizes** (`tools/size_report.py`). These two tables are of v1.20's code, which is what the branch held at the time; today's main is measured further down.

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

- The image built here and v1.20's image from the other PC give the same table to 0.1 KB. That was read as "main has no code change since v1.20". It was the local `main` that had none; the remote's had 11 commits more (below).
- **A fault found in the report, and fixed.** Its first version told the parts apart by words anywhere in an object's path. On this PC the compiler lives in a folder named `.pico-sdk`, so the C runtime (70 KB) was counted as the Pico SDK; the same loose matching had put `mui_menu.c` under u8g2 and `freertos_hooks.c` under FreeRTOS. A part is now told by where its source sits in the repository, and anything from outside the tree is the compiler's runtime. The table no longer depends on where anything is installed.
- The numbers in the first drafts of ADR-005 and ADR-010 came from the loose version and were corrected (the application is 52 KB on Pico W, not 48; u8g2 and MUI 32 KB, not 35).

**The CI run.** The owner said to push the branch and start a run by hand (CI runs on pushes to `main`, on PRs, and when started by hand).

- **The first run failed, and not because of this branch.** It stopped in "Build Pico2 W Package", before the size report:
  - `ubuntu-latest` is now `ubuntu-26.04`, whose own compiler is GCC 15.2;
  - the Pico SDK builds its `pioasm` tool with the runner's compiler, and SDK 2.1.1's `pio_types.h` uses `uint8_t` without including `<cstdint>`, which GCC 15 no longer lets pass;
  - main's last green run was 2026-07-28, on the image before. So CI has been broken on main since the image moved, for any PR.
- **The fix:** the runner is pinned to `ubuntu-24.04`, the image the build last passed on, with a comment that says why and when to move it (when the SDK is updated, which is a dependency ADR of its own).
- **The second run passed** ([run 43410387723](https://github.com/eamars/OpenTrickler-RP2040-Controller/actions/runs/43410387723)), both boards, with both tables in the job summary and in the log:

| In CI | Pico 2 W | Pico W |
|---|---|---|
| Flash | 624.3 KB | 645.1 KB |
| RAM at link time | 195.5 KB | 195.1 KB |
| The report's total against `app.bin` | −0.1% | −0.1% |
| C and C++ runtime | 29.1 KB | 27.3 KB |

- The report works on a map made by Make on Linux as it does on one made by Ninja on Windows: same parts, object names ending `.o` there and `.obj` here.
- **The same code gives two images.** CI's are about 40 KB smaller than this PC's, all of it in the C and C++ runtime. The compilers differ: CI installs Ubuntu 24.04's `gcc-arm-none-eabi` (13.2.1), this PC has Arm's 14.2.Rel1, which is what `configure_env.ps1` names. The 37 KB name demangler is in the image from Arm's compiler and not in CI's.
- **The releases are not CI's builds.** v1.20's release files are 1,323 KB and 1,374 KB as `.uf2`, which is 661 KB and 687 KB of image: the size of a build with Arm's compiler on a PC, not of CI's (625 KB and 646 KB). ADR-005 said otherwise and is corrected. Which compiler is the reference for a release is for ADR-004's release job to settle.

## The branch was cut from an old main

Found 2026-11-13, while opening the PR for the CI pin.

- **What happened.** The local `main` was 11 commits behind the remote's, and the agent never ran `git fetch`. The survey, all ten ADRs and steps 1 and 2 were done against the code of 2026-04-28. The PR and issue lists came from GitHub and were current.
- **What main had gained:**
  - a simulated scale, as scale driver 9, in the screen menu and the web page (PR #107);
  - a short reverse of the coarse trickler when it stops, with three new stored settings (PR #108);
  - the reset button as "up one level" in the menu (PR #110);
  - a fix for REST values that contain `&` (PR #112);
  - `setup_toolchain.sh` and `configure_env.sh` for macOS, and `.toolchain/` in `.gitignore`.
- **What was done:**
  - main merged into this branch. One conflict, in `.gitignore`; both sides kept;
  - `setup_toolchain.ps1` moved from `tools/` to the root, beside the macOS script and `configure_env.ps1`;
  - both boards rebuilt and measured again (below);
  - the facts corrected in ADR-001, 002, 003, 005, 006, 009 and 010;
  - "fetch first" written as the first rule in `AGENTS.md`.
- **What it changes in this ADR.** Step 5 planned a simulated scale and main has one. That is F2 in the README, with a proposal, and it waits for the owner.

**Today's main, built on this PC:**

| | Pico 2 W | Pico W |
|---|---|---|
| Flash | 667.7 KB of 4,096 | 693.0 KB of 2,048 |
| RAM at link time | 195.5 KB of 512 | 195.0 KB of 256 |
| The report's total against `app.bin` | +0.3% | +0.3% |
| The radio: cyw43's driver and its firmware | 249.9 KB | 251.6 KB |
| lwIP | 93.4 KB | 105.4 KB |
| Web pages | 91.2 KB | 91.3 KB |
| C and C++ runtime | 69.0 KB | 71.7 KB |
| Application | 51.4 KB | 53.7 KB |
| Pico SDK | 50.3 KB | 52.9 KB |
| FreeRTOS | 29.5 KB | 28.4 KB |
| u8g2 and MUI | 27.9 KB | 31.6 KB |
| HTTP server | 4.0 KB | 4.8 KB |
| Trinamic | 1.1 KB | 1.6 KB |

- 4 KB more than v1.20 on each board: about 2 KB of application, 1.5 KB of pages, the rest the HTTP server.

**The CI pin on main.** The pin went to main on its own as PR #114 and was merged by the agent, under the owner's rule of the same day that an update that is not a feature may be merged without asking.

## Steps 3 to 6

Not started. They need the owner's word that the bench is ready. Step 5 also needs the owner's word on F2.
