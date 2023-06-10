
![12864_main_menu](resources/main_menu_screen_mirror.png)

# OpenTrickler RP2040 Controller 
This repo is for the firmware that utilises the Raspberry Pi RP2040 micro controller OpenTrickler RP2040 Controller.

 
## Prerequistes  
[Git](https://gitforwindows.org/) and [Pico-SDK](https://github.com/raspberrypi/pico-setup-windows/releases/download/v0.5.1/pico-setup-windows-x64-standalone.exe) are required to build the firmware. 
 
## Setting Up Firmware 
 Using Git Bash clone the repository   
~~~javascript  
 git clone https://github.com/eamars/OpenTrickler-RP2040-Controller
~~~  
Next change to the cloned directory
~~~javascript  
 cd OpenTrickler-RP2040-Controller
~~~  
Next use git to initalise the required submodules
~~~javascript  
 git submodule init
~~~  
Now using git clone all submodules
~~~javascript  
 git submodule update --init --recursive
~~~  

## Setting Up Libraries
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
## Compiling the Firmware
Open Pico-VisualStudioCode and open the OpenTrickler-RP2040-Controller folder then navigate to the cmake plugin and click Build All Projects.
