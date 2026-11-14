# Puts the toolchain that setup_toolchain.ps1 fetched on the path. The versions are in toolchain.env (ADR-011).

$V = @{}
foreach ($line in Get-Content (Join-Path $PSScriptRoot 'toolchain.env')) {
    if ($line -match '^([A-Z_]+)=(.+)$') { $V[$Matches[1]] = $Matches[2].Trim() }
}
# The VS Code extension names the compiler's folder 15_2_Rel1 for Arm's release 15.2.rel1
$ArmFolder = ($V.ARM_GCC -replace '\.', '_') -replace 'rel', 'Rel'
$PicoSdkRoot = "$env:USERPROFILE\.pico-sdk"

# The compiler
$env:PICO_TOOLCHAIN_PATH = "$PicoSdkRoot\toolchain\$ArmFolder"
$env:Path = "$env:PICO_TOOLCHAIN_PATH\bin;" + $env:Path

# Ninja and CMake
$env:Path = "$PicoSdkRoot\ninja\v$($V.NINJA);" + $env:Path
$env:Path = "$PicoSdkRoot\cmake\v$($V.CMAKE)\bin;" + $env:Path

# picotool and pioasm, ready-made: the SDK finds them on the path and does not build them
$env:Path = "$PicoSdkRoot\picotool\$($V.PICO_TOOLS)\picotool;" + $env:Path
$env:Path = "$PicoSdkRoot\tools\$($V.PICO_TOOLS)\pioasm;" + $env:Path

# OpenOCD and its scripts
$OPENOCD_PATH = "$PicoSdkRoot\openocd\$($V.OPENOCD)"
$env:Path = "$OPENOCD_PATH;" + $env:Path
$env:OPENOCD_SCRIPTS = "$OPENOCD_PATH\scripts"
