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

Assert-File $HexPath "Missing HEX file. Build the $Config preset first"

$Programmer = Find-Programmer
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
