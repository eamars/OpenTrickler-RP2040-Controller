. .\configure_env.ps1

# Configure CMake
cmake -B build_w -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPICO_BOARD=pico_w
cmake --build build_w --config Debug