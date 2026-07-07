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

& $cmakeExe @args
exit $LASTEXITCODE
