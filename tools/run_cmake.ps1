$ErrorActionPreference = "Stop"

$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmakeCommand) {
  $cmakeExe = $cmakeCommand.Source
} else {
  $bundleRoot = Join-Path $env:LOCALAPPDATA "stm32cube\bundles\cmake"
  $cmakeExe = Get-ChildItem $bundleRoot -Recurse -Filter cmake.exe -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending |
    Select-Object -First 1 -ExpandProperty FullName
}

if (-not $cmakeExe) {
  throw "cmake.exe was not found in PATH or $env:LOCALAPPDATA\stm32cube\bundles\cmake"
}

$gccCommand = Get-Command arm-none-eabi-gcc.exe -ErrorAction SilentlyContinue
if (-not $gccCommand) {
  $gccBundleRoot = Join-Path $env:LOCALAPPDATA "stm32cube\bundles\gnu-tools-for-stm32"
  $gccExe = Get-ChildItem $gccBundleRoot -Recurse -Filter arm-none-eabi-gcc.exe -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending |
    Select-Object -First 1 -ExpandProperty FullName

  if ($gccExe) {
    $env:PATH = "$(Split-Path -Parent $gccExe);$env:PATH"
  }
}

$ninjaCommand = Get-Command ninja.exe -ErrorAction SilentlyContinue
if (-not $ninjaCommand) {
  $ninjaBundleRoot = Join-Path $env:LOCALAPPDATA "stm32cube\bundles\ninja"
  $ninjaExe = Get-ChildItem $ninjaBundleRoot -Recurse -Filter ninja.exe -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending |
    Select-Object -First 1 -ExpandProperty FullName

  if ($ninjaExe) {
    $env:PATH = "$(Split-Path -Parent $ninjaExe);$env:PATH"
  }
}

& $cmakeExe @args
exit $LASTEXITCODE
