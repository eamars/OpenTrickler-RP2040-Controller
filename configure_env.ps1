# Add compiler to the path
$env:Path = "${env:USERPROFILE}/.pico-sdk/toolchain/14_2_Rel1/bin;" + $env:Path

# Specify Ninja path
$env:Path = "$env:USERPROFILE\.pico-sdk\ninja\v1.12.1;" + $env:Path

# Specify CMake path
$env:Path = "$env:USERPROFILE\.pico-sdk\cmake\v3.31.5\bin;" + $env:Path

# Specify picotool path
$env:Path = "$env:USERPROFILE\.pico-sdk\picotool\2.2.0\picotool;" + $env:Path

# Specify pioasm path
$env:Path = "$env:USERPROFILE\.pico-sdk\tools\2.2.0\pioasm;" + $env:Path

# Specify OpenOCD Path
$OPENOCD_PATH = "$env:USERPROFILE\.pico-sdk\openocd\0.12.0+dev"
$env:Path = "$OPENOCD_PATH;" + $env:Path

# Specify OpenOCD search path
$env:OPENOCD_SCRIPTS = "$OPENOCD_PATH\scripts"

# Specify PICO_TOOLCHAIN_PATH
$env:PICO_TOOLCHAIN_PATH="$env:USERPROFILE\.pico-sdk\toolchain\14_2_Rel1"

# Load the MSVC host-compiler environment (cl.exe, INCLUDE, LIB, etc.).
# The Pico SDK builds pioasm from source as a host tool (see
# library/pico-sdk/tools/Findpioasm.cmake), which needs a native Windows
# compiler completely separate from the arm-none-eabi-gcc cross-compiler
# above. VSCode's CMake Tools extension finds this for you automatically;
# a plain PowerShell session needs it loaded explicitly.
$vswherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswherePath) {
    $vsInstallPath = & $vswherePath -latest -products * -property installationPath
    if ($vsInstallPath) {
        $vcvars64Path = Join-Path $vsInstallPath "VC\Auxiliary\Build\vcvars64.bat"
        if (Test-Path $vcvars64Path) {
            cmd /c "`"$vcvars64Path`" && set" | ForEach-Object {
                if ($_ -match "^([^=]+)=(.*)$") {
                    Set-Item -Path "env:$($matches[1])" -Value $matches[2]
                }
            }
        }
        else {
            Write-Warning "vcvars64.bat not found under '$vsInstallPath' -- host tool builds (e.g. pioasm) may fail to find a C++ compiler."
        }
    }
    else {
        Write-Warning "No Visual Studio / Build Tools installation found via vswhere -- host tool builds (e.g. pioasm) may fail to find a C++ compiler."
    }
}
else {
    Write-Warning "vswhere.exe not found -- install Visual Studio Build Tools (Desktop development with C++) or host tool builds (e.g. pioasm) may fail to find a C++ compiler."
}