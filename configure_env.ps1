# Add compiler to the path
$env:Path = "${env:USERPROFILE}/.pico-sdk/toolchain/14_2_Rel1/bin;" + $env:Path

# Specify Ninja path
$env:Path = "$env:USERPROFILE\.pico-sdk\ninja\v1.12.1;" + $env:Path

# Specify CMake path
$env:Path = "$env:USERPROFILE\.pico-sdk\cmake\v3.31.5\bin;" + $env:Path

# Specify picotool path
$env:Path = "$env:USERPROFILE\.pico-sdk\picotool\2.1.1\picotool;" + $env:Path

# Specify pioasm path
$env:Path = "$env:USERPROFILE\.pico-sdk\tools\2.1.1\pioasm;" + $env:Path

# Specify OpenOCD Path
$OPENOCD_PATH = "$env:USERPROFILE\.pico-sdk\openocd\0.12.0+dev"
$env:Path = "$OPENOCD_PATH;" + $env:Path

# Specify OpenOCD search path
$env:OPENOCD_SCRIPTS = "$OPENOCD_PATH\scripts"

# Specify PICO_TOOLCHAIN_PATH
$env:PICO_TOOLCHAIN_PATH="$env:USERPROFILE\.pico-sdk\toolchain\14_2_Rel1"