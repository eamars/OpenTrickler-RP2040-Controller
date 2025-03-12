. .\configure_env.ps1

# Configure CMake
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPICO_BOARD=pico2_w
# cmake --build build --config Debug