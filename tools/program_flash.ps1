param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",

    [string]$ProgrammerPath = "",

    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$BuildDir = Join-Path $Root "build\$Config"
$HexPath = Join-Path $BuildDir "U5_LIGHT.hex"
$RunCMakePath = Join-Path $Root "tools\run_cmake.ps1"
$PowerShellExe = (Get-Command powershell.exe -ErrorAction Stop).Source

function Assert-File {
    param(
        [string]$Path,
        [string]$Message
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Message`: $Path"
    }
}

function Find-Programmer {
    if ($ProgrammerPath) {
        Assert-File $ProgrammerPath "STM32_Programmer_CLI.exe was not found"
        return (Resolve-Path -LiteralPath $ProgrammerPath).Path
    }

    $bundleRoot = Join-Path $env:LOCALAPPDATA "stm32cube\bundles\programmer"
    if (Test-Path -LiteralPath $bundleRoot) {
        $candidate = Get-ChildItem -LiteralPath $bundleRoot -Recurse -Filter STM32_Programmer_CLI.exe -ErrorAction SilentlyContinue |
            Sort-Object FullName -Descending |
            Select-Object -First 1
        if ($candidate) {
            return $candidate.FullName
        }
    }

    $fromPath = Get-Command STM32_Programmer_CLI.exe -ErrorAction SilentlyContinue
    if ($fromPath) {
        return $fromPath.Source
    }

    throw "STM32_Programmer_CLI.exe was not found. Install STM32CubeProgrammer or pass -ProgrammerPath."
}

function Invoke-FirmwareBuild {
    Assert-File $RunCMakePath "CMake launcher script was not found"

    Write-Host "Refreshing PC time and configuring $Config firmware..."
    & $PowerShellExe -NoProfile -ExecutionPolicy Bypass -File $RunCMakePath --preset $Config
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed with exit code $LASTEXITCODE"
    }

    Write-Host "Building $Config firmware with the refreshed PC time..."
    & $PowerShellExe -NoProfile -ExecutionPolicy Bypass -File $RunCMakePath --build --preset $Config
    if ($LASTEXITCODE -ne 0) {
        throw "Firmware build failed with exit code $LASTEXITCODE"
    }
}

function Format-CommandLine {
    param(
        [string]$Exe,
        [string[]]$CliArgs
    )

    $quoted = @($Exe) + $CliArgs | ForEach-Object {
        if ($_ -match "\s") {
            '"' + $_ + '"'
        } else {
            $_
        }
    }
    return ($quoted -join " ")
}

$Programmer = Find-Programmer
Invoke-FirmwareBuild
Assert-File $HexPath "Missing HEX file after building the $Config preset"

$CliArgs = @(
    "-c", "port=SWD", "mode=UR",
    "-w", $HexPath,
    "-v",
    "-rst"
)

Write-Host (Format-CommandLine $Programmer $CliArgs)
if ($DryRun) {
    exit 0
}

& $Programmer @CliArgs
if ($LASTEXITCODE -ne 0) {
    throw "Flash failed with exit code $LASTEXITCODE"
}
