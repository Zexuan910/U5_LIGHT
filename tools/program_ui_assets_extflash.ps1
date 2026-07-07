param(
    [string]$AssetBin = "Assets\extflash\u5_ui_assets_extflash.bin",
    [string]$Address = "0x90000000",
    [string]$ExternalLoader = "",
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$workspace = Split-Path -Parent $PSScriptRoot
$assetPath = Join-Path $workspace $AssetBin
if (-not (Test-Path $assetPath)) {
    throw "Asset package not found: $assetPath"
}

$programmerCandidates = @(
    "$env:LOCALAPPDATA\stm32cube\bundles\programmer\2.22.0+st.1\bin\STM32_Programmer_CLI.exe",
    "$env:LOCALAPPDATA\stm32cube\bundles\programmer\2.22.0\bin\STM32_Programmer_CLI.exe",
    "$env:LOCALAPPDATA\stm32cube\bundles\programmer\2.21.0\bin\STM32_Programmer_CLI.exe"
)

$programmer = $programmerCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $programmer) {
    throw "STM32_Programmer_CLI.exe was not found."
}

$args = @("-c", "port=SWD", "mode=UR")
if ($ExternalLoader -ne "") {
    $loaderPath = $ExternalLoader
    if (-not [System.IO.Path]::IsPathRooted($loaderPath)) {
        $loaderPath = Join-Path (Split-Path -Parent $programmer) ("ExternalLoader\" + $ExternalLoader)
    }
    if (-not (Test-Path $loaderPath)) {
        throw "External loader not found: $loaderPath"
    }
    $args += @("-el", $loaderPath)
} else {
    Write-Warning "No external loader was specified. This usually cannot program board external Flash."
}

$args += @("-w", $assetPath, $Address, "-v", "-rst")

Write-Host "$programmer $($args -join ' ')"
if (-not $DryRun) {
    & $programmer @args
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}
