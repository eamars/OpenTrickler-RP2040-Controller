# OpenTrickler RP2040 Controller - with OTA update capability
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

You can download the pre-built firmware based on the latest release from above link. Similar to flashing other RP2040 firmware, you need to put the Pico W into the bootloader mode by pressing BOOTSEL button and plug in the micro-USB cable. Then you can copy the .uf2 file from the package to the pico. Shortly after the Pico W will be programmed automatically. Alternatively, use the OTA update feature described below.



## Build OpenTrickler firmware from source
### Prerequistes  
[Git](https://gitforwindows.org/) and [Pico-SDK](https://github.com/raspberrypi/pico-setup-windows/releases/download/v0.5.1/pico-setup-windows-x64-standalone.exe) are required to build the firmware. 
 
### Setting Up Firmware 
 Using Git Bash clone this repository   

    git clone <URL>

Next change to the cloned directory

    cd OpenTrickler-RP2040-Controller

Next use git to initalise the required submodules

    git submodule init

Now using git clone all submodules

    git submodule update --init --recursive
 

### Setting Up Libraries
Using the Pico-Developer window navigate to the cloned directory.
~~~javascript  
 cd Path:\to\cloned\repository
~~~  
Navigate to the build folder.
~~~javascript  
 cd build
~~~
Then run the following comand
~~~javascript  
cmake .. -DPICO_BOARD=pico_w -DCMAKE_BUILD_TYPE=Debug
~~~

### Compiling the Firmware
Open Pico-VisualStudioCode and open the OpenTrickler-RP2040-Controller folder then navigate to the cmake plugin.

First time compilation: Compile "App" only by clicking the Build Icon next to "app [app.elf]".
After that, hit "Build All Projects" to let the whole project bake together. From now on, "Build All Projects" is good to go.


### Flashing the Firmware
The first time you have to flash the bootloader by flashing "picowota_app.uf2" via Pico's USB bootloader (the way you did up to now).

On any further flashing, one can use serial-flash from usedbytes: https://github.com/usedbytes/serial-flash
Read on usedbytes' repository on how to obtain it.

Once serial-flash is working, and OpenTrickler is in Bootloader (via menu, Settings -> Bootloader), one can use serial-flash_app.bat to send app.elf over the air to the OpenTrickler. You may need to configure the correct IP address.

If you configured WiFi via OpenTrickler Web Interface, it will use the same credentials for the bootloader. 

If not, OpenTrickler Bootloader will create a WiFi AP with following credentials:

SSID: OpenTricklerBootloader

PW: opentrickler

Credentials for access point can be changed in CMakeLists.txt.


### Known Issues
For Windows users: If necessary, get precompiled PIOASM.exe and ELF2UF2.exe from https://sourceforge.net/projects/rpi-pico-utils/ or configure CMakeLists.txt around line 50 according to your needs. I write this, because full "Windows 10 SDK" is required to build PIOASM and ELF2UF2.
