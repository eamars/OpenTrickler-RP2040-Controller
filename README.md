# OpenTrickler RP2040 Controller 
This repo is for the firmware that utilises the Raspberry Pi RP2040 micro controller OpenTrickler RP2040 Controller.

Join our [discord server](https://discord.gg/ZhdThA2vrW) for help and development information. 

## Get Started
### Use with mini 12864 display
1. From the main menu, select "Start".

    ![12864_main_menu](resources/main_menu_screen_mirror.png)
2. Provide the target charge weight in grain then press Next to continue.

    ![12864_select_charge_weight](resources/select_weight_screen_mirror.png)
3. Remember to put pan on the scale. 

    ![12864_waring_put_pan_on_scale](resources/put_pan_warning_screen_mirror.png)
4. Wait for scale to stable at 0. Or press the rotary button to force Re-zero. 

    ![12864_wait_for_zero](resources/wait_for_zero_screen_mirror.png)
5. Wait for charge to reach the set point

    ![12864_wait for charge](resources/wait_for_charge_screen_mirror.png)
6. Once the charge set point is reached, remove the pan. The program shall restart from step 4.

    ![12864_wait_for_cup_removal](resources/wait_for_cup_removal.png)

## Pre-build firmware
[![Auto Build](https://github.com/eamars/OpenTrickler-RP2040-Controller/actions/workflows/cmake.yml/badge.svg)](https://github.com/eamars/OpenTrickler-RP2040-Controller/actions/workflows/cmake.yml)

You can download the pre-built firmware based on the latest release from above link. Similar to flashing other RP2040 firmware, you need to put the Pico W into the bootloader mode by pressing BOOTSEL button and plug in the micro-USB cable. Then you can copy the .uf2 file from the package to the pico. Shortly after the Pico W will be programmed automatically. 

## Build OpenTrickler firmware from source on Windows

Reference: https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf

### Prerequistes  
[Git](https://gitforwindows.org/) and [VSCode](https://code.visualstudio.com/) are required to build the firmware. To install build dependencies, you will need to use [VSCode Raspberry Pi Pico extension](https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico) and create an pico-example project (any project will trigger the download of pico-sdk, a collection of tools required to build the firmware locally). 

Then you can verify the installation of pico-sdk by inspecting the path from `C:\Users\<user name>\.pico-sdk`. 
![pico_sdk_path](resources/pico_sdk_path.png)
 
### Downloading Source Code
From PowerShell, execute below command to fetch the source code: 

    git clone https://github.com/eamars/OpenTrickler-RP2040-Controller

Next change to the cloned directory

    cd OpenTrickler-RP2040-Controller

Next use git to initalise the required submodules

    git submodule init

Now using git clone all submodules. It may take up to 5 minutes to clone all required submodules.

    git submodule update --init --recursive
 
### Configure CMake
Open the PowerShell, run the below script to load required environment variables: 

    .\configure_env.ps1

To build firmware for Pico W, from the same PowerShell session, run below command:

    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPICO_BOARD=pico_w

To build firmware for Pico 2W, from the same PowerShell session, run below command:

    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPICO_BOARD=pico2_w

### Build Firmware
From the same workspace root directory, run the below command to build the firmware from source code into the `build` directory: 

    cmake --build build --config Debug

On success, you can find the app.uf2 from `<workspace_root>/build/` directory. 

### Use VSCode
You need to call VScode from script to pre-configure environment variables. You can simply call

    .\run_vscode.ps1

The VSCode cmake plugin is pre-configured to build for Pico 2W by default. You can change the build config to Pico W by modifying `<workspace_root>.vscode/settings.json`. 
