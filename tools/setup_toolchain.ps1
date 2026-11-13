<#
Fetches the toolchain this repository builds with into %USERPROFILE%\.pico-sdk, in the layout the VS Code Pico
extension makes, so configure_env.ps1 works unchanged (ADR-001, decision 4).

A part that is already there is left alone. Nothing outside %USERPROFILE%\.pico-sdk is touched.

Usage:
    .\tools\setup_toolchain.ps1          fetch what is missing
    .\tools\setup_toolchain.ps1 -List    print what would be fetched, and fetch nothing
#>
param(
    [switch] $List
)

$ErrorActionPreference = 'Stop'

$PicoSdkRoot = Join-Path $env:USERPROFILE '.pico-sdk'

# Dest is relative to $PicoSdkRoot. Probe is the file that shows the part is there, relative to Dest.
$Parts = @(
    @{
        Name = 'Arm GNU toolchain 14.2.Rel1 (arm-none-eabi), about 298 MB'
        Url = 'https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-mingw-w64-x86_64-arm-none-eabi.zip'
        Dest = 'toolchain\14_2_Rel1'
        Probe = 'bin\arm-none-eabi-gcc.exe'
    },
    @{
        Name = 'CMake 3.31.5, about 44 MB'
        Url = 'https://github.com/Kitware/CMake/releases/download/v3.31.5/cmake-3.31.5-windows-x86_64.zip'
        Dest = 'cmake\v3.31.5'
        Probe = 'bin\cmake.exe'
    },
    @{
        Name = 'Ninja 1.12.1'
        Url = 'https://github.com/ninja-build/ninja/releases/download/v1.12.1/ninja-win.zip'
        Dest = 'ninja\v1.12.1'
        Probe = 'ninja.exe'
    },
    @{
        Name = 'picotool 2.1.1'
        Url = 'https://github.com/raspberrypi/pico-sdk-tools/releases/download/v2.1.1-3/picotool-2.1.1-x64-win.zip'
        Dest = 'picotool\2.1.1'
        Probe = 'picotool\picotool.exe'
    },
    @{
        Name = 'pioasm 2.1.1 (pico-sdk-tools)'
        Url = 'https://github.com/raspberrypi/pico-sdk-tools/releases/download/v2.1.1-3/pico-sdk-tools-2.1.1-x64-win.zip'
        Dest = 'tools\2.1.1'
        Probe = 'pioasm\pioasm.exe'
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
