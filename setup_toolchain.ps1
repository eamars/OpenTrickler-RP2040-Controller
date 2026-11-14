<#
Fetches the toolchain this repository builds with into %USERPROFILE%\.pico-sdk, in the layout the VS Code Pico
extension makes, so configure_env.ps1 finds it (ADR-001, decision 4). The versions are in toolchain.env
(ADR-011).

A part that is already there is left alone. Nothing outside %USERPROFILE%\.pico-sdk is touched.

Usage:
    .\setup_toolchain.ps1          fetch what is missing
    .\setup_toolchain.ps1 -List    print what would be fetched, and fetch nothing

Linux (CI) has its own script, setup_toolchain.sh.
#>
param(
    [switch] $List
)

$ErrorActionPreference = 'Stop'

$PicoSdkRoot = Join-Path $env:USERPROFILE '.pico-sdk'

# The versions, from toolchain.env: one NAME=value a line
$V = @{}
foreach ($line in Get-Content (Join-Path $PSScriptRoot 'toolchain.env')) {
    if ($line -match '^([A-Z_]+)=(.+)$') { $V[$Matches[1]] = $Matches[2].Trim() }
}
# The VS Code extension names the compiler's folder 15_2_Rel1 for Arm's release 15.2.rel1
$ArmFolder = ($V.ARM_GCC -replace '\.', '_') -replace 'rel', 'Rel'
$ToolsUrl = "https://github.com/raspberrypi/pico-sdk-tools/releases/download/$($V.PICO_SDK_TOOLS_RELEASE)"

# Dest is relative to $PicoSdkRoot. Probe is the file that shows the part is there, relative to Dest.
$Parts = @(
    @{
        Name = "Arm GNU toolchain $($V.ARM_GCC) (arm-none-eabi), about 300 MB"
        Url = "https://developer.arm.com/-/media/Files/downloads/gnu/$($V.ARM_GCC)/binrel/arm-gnu-toolchain-$($V.ARM_GCC)-mingw-w64-x86_64-arm-none-eabi.zip"
        Dest = "toolchain\$ArmFolder"
        Probe = 'bin\arm-none-eabi-gcc.exe'
    },
    @{
        Name = "CMake $($V.CMAKE)"
        Url = "https://github.com/Kitware/CMake/releases/download/v$($V.CMAKE)/cmake-$($V.CMAKE)-windows-x86_64.zip"
        Dest = "cmake\v$($V.CMAKE)"
        Probe = 'bin\cmake.exe'
    },
    @{
        Name = "Ninja $($V.NINJA)"
        Url = "https://github.com/ninja-build/ninja/releases/download/v$($V.NINJA)/ninja-win.zip"
        Dest = "ninja\v$($V.NINJA)"
        Probe = 'ninja.exe'
    },
    @{
        Name = "picotool $($V.PICO_TOOLS)"
        Url = "$ToolsUrl/picotool-$($V.PICO_TOOLS)-x64-win.zip"
        Dest = "picotool\$($V.PICO_TOOLS)"
        Probe = 'picotool\picotool.exe'
    },
    @{
        Name = "pioasm $($V.PICO_TOOLS) (pico-sdk-tools)"
        Url = "$ToolsUrl/pico-sdk-tools-$($V.PICO_TOOLS)-x64-win.zip"
        Dest = "tools\$($V.PICO_TOOLS)"
        Probe = 'pioasm\pioasm.exe'
    },
    @{
        # Flashes and reads a unit through a debug probe (ADR-001, decisions 3 and 4)
        Name = "OpenOCD $($V.OPENOCD) (Raspberry Pi build), about 8 MB"
        Url = "https://github.com/raspberrypi/pico-sdk-tools/releases/download/$($V.OPENOCD_RELEASE)/openocd-$($V.OPENOCD)-x64-win.zip"
        Dest = "openocd\$($V.OPENOCD)"
        Probe = 'openocd.exe'
    }
)


# The folder inside an unpacked archive whose contents belong in Dest: the one that holds Probe. Some archives
# hold their files directly and some wrap them in one folder, so the probe decides, not an assumption per archive.
function Find-ArchiveRoot($Unpacked, $Probe, $Name) {
    if (Test-Path (Join-Path $Unpacked $Probe)) {
        return $Unpacked
    }
    $folders = @(Get-ChildItem $Unpacked -Directory)
    if ($folders.Count -eq 1 -and (Test-Path (Join-Path $folders[0].FullName $Probe))) {
        return $folders[0].FullName
    }
    throw "$Probe was not found in the archive of $Name. Its top level holds: $((Get-ChildItem $Unpacked).Name -join ', ')"
}


function Install-Part($Part) {
    $dest = Join-Path $PicoSdkRoot $Part.Dest
    $work = Join-Path $env:TEMP ('pico_toolchain_' + [guid]::NewGuid().ToString('N'))
    $archive = Join-Path $work 'archive.zip'
    $unpacked = Join-Path $work 'unpacked'
    New-Item -ItemType Directory -Force $unpacked | Out-Null

    try {
        # curl.exe and tar.exe ship with Windows 10 and later; both are much faster than the PowerShell cmdlets
        # on an archive of this size
        curl.exe --fail --location --retry 3 --silent --show-error --output $archive $Part.Url
        if ($LASTEXITCODE -ne 0) { throw "Download failed: $($Part.Url)" }
        tar.exe -xf $archive -C $unpacked
        if ($LASTEXITCODE -ne 0) { throw "Unpacking failed: $archive" }

        $source = Find-ArchiveRoot $unpacked $Part.Probe $Part.Name
        New-Item -ItemType Directory -Force $dest | Out-Null
        Get-ChildItem $source -Force | Move-Item -Destination $dest
    }
    finally {
        # Whatever happened, no archive or half-unpacked tree stays behind in the temp folder
        Remove-Item -Recurse -Force $work
    }
}


foreach ($part in $Parts) {
    $dest = Join-Path $PicoSdkRoot $part.Dest
    if (Test-Path (Join-Path $dest $part.Probe)) {
        Write-Output "Have    $($part.Name) in $dest"
        continue
    }
    Write-Output "Fetch   $($part.Name)"
    Write-Output "        from $($part.Url)"
    Write-Output "        into $dest"
    if ($List) { continue }
    if (Test-Path $dest) {
        throw "$dest exists but $($part.Probe) is not in it. Remove the folder and run again."
    }
    Install-Part $part
    Write-Output "Done    $($part.Name)"
}
