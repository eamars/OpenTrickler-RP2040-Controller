# OpenTrickler RP2040 Controller 
This repo is for the firmware that utilises the Raspberry Pi RP2040 micro controller OpenTrickler RP2040 Controller.

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


## Build OpenTrickler firmware from source
### Prerequistes  
[Git](https://gitforwindows.org/) and [Pico-SDK](https://github.com/raspberrypi/pico-setup-windows/releases/download/v0.5.1/pico-setup-windows-x64-standalone.exe) are required to build the firmware. 
 
### Setting Up Firmware 
 Using Git Bash clone the repository   

    git clone https://github.com/eamars/OpenTrickler-RP2040-Controller

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
Open Pico-VisualStudioCode and open the OpenTrickler-RP2040-Controller folder then navigate to the cmake plugin and click Build All Projects.

## Pre-build firmware
[![Auto Release Build](https://github.com/eamars/OpenTrickler-RP2040-Controller/actions/workflows/cmake.yml/badge.svg)](https://github.com/eamars/OpenTrickler-RP2040-Controller/actions/workflows/cmake.yml)
