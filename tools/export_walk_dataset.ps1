param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern("^[A-Za-z0-9_]+$")]
    [string]$Label,

    [string]$OutDir = "",

    [string]$Category = "",

    [ValidateRange(1, 64)]
    [int]$TrainingLimit = 8,

    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",

    [int]$Capacity = 524288,
    [int]$Window = 64,
    [int]$Stride = 16,

    [ValidateRange(0, 63)]
    [int]$SessionFromLatest = 0,

    [switch]$ListSessions,

    [switch]$EraseStoredData,

    [string]$ProgrammerPath = "",
    [string]$NmPath = "",
    [string]$PythonPath = ""
)

$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$BuildDir = Join-Path $Root "build\$Config"
$ElfPath = Join-Path $BuildDir "U5_LIGHT.elf"
$RawDir = Join-Path $Root "Dataset\raw"
$SamplesBin = Join-Path $RawDir "$Label`_samples.bin"
$MetaBin = Join-Path $RawDir "$Label`_meta.bin"

function Assert-File {
    param([string]$Path, [string]$Message)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Message`: $Path"
    }
}

function Resolve-DatasetCategory {
    param([string]$RequestedCategory, [string]$DatasetLabel)

    $KnownCategories = @("normal_walk", "fast_walk", "still", "rope")
    if ($RequestedCategory) {
        $NormalizedCategory = $RequestedCategory.ToLowerInvariant()
        if ($KnownCategories -notcontains $NormalizedCategory) {
            throw "Unsupported category '$RequestedCategory'. Use normal_walk, fast_walk, still, or rope."
        }
        return $NormalizedCategory
    }

    $NormalizedLabel = $DatasetLabel.ToLowerInvariant()
    if (($NormalizedLabel -eq "walk") -or $NormalizedLabel.StartsWith("normal_walk")) {
        return "normal_walk"
    }
    if ($NormalizedLabel.StartsWith("fast_walk")) {
        return "fast_walk"
    }
    if ($NormalizedLabel.StartsWith("still")) {
        return "still"
    }
    if ($NormalizedLabel.StartsWith("rope")) {
        return "rope"
    }

    throw "Cannot infer a category from label '$DatasetLabel'. Pass -Category normal_walk, fast_walk, still, or rope."
}

function Test-ContainsNanoEdgeSignal {
    param([string]$Directory)

    $Signal = Get-ChildItem -LiteralPath $Directory -File -Filter "*.csv" -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Name -notmatch "_continuous\.csv$" -and
            $_.Name -notmatch "metadata" -and
            $_.Name -notmatch "summary"
        } |
        Select-Object -First 1
    return ($null -ne $Signal)
}

function Get-TrainingDatasetCount {
    param([string]$CategoryDirectory)

    if (-not (Test-Path -LiteralPath $CategoryDirectory)) {
        return 0
    }

    return @(
        Get-ChildItem -LiteralPath $CategoryDirectory -Directory -ErrorAction SilentlyContinue |
            Where-Object { Test-ContainsNanoEdgeSignal $_.FullName }
    ).Count
}

function Get-UniqueDirectoryPath {
    param([string]$Parent, [string]$Name)

    $Candidate = Join-Path $Parent $Name
    $Suffix = 2
    while (Test-Path -LiteralPath $Candidate) {
        $Candidate = Join-Path $Parent "$Name`_$Suffix"
        $Suffix++
    }
    return $Candidate
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

    throw "STM32_Programmer_CLI.exe was not found."
}

function Find-Nm {
    if ($NmPath) {
        Assert-File $NmPath "arm-none-eabi-nm.exe was not found"
        return (Resolve-Path -LiteralPath $NmPath).Path
    }

    $fromPath = Get-Command arm-none-eabi-nm.exe -ErrorAction SilentlyContinue
    if ($fromPath) {
        return $fromPath.Source
    }

    $bundleRoot = Join-Path $env:LOCALAPPDATA "stm32cube\bundles\gnu-tools-for-stm32"
    if (Test-Path -LiteralPath $bundleRoot) {
        $candidate = Get-ChildItem -LiteralPath $bundleRoot -Recurse -Filter arm-none-eabi-nm.exe -ErrorAction SilentlyContinue |
            Sort-Object FullName -Descending |
            Select-Object -First 1
        if ($candidate) {
            return $candidate.FullName
        }
    }

    throw "arm-none-eabi-nm.exe was not found."
}

function Find-Python {
    if ($PythonPath) {
        Assert-File $PythonPath "python.exe was not found"
        return (Resolve-Path -LiteralPath $PythonPath).Path
    }

    $fromPath = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($fromPath) {
        return $fromPath.Source
    }

    $codexPython = Join-Path $env:USERPROFILE ".cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"
    if (Test-Path -LiteralPath $codexPython) {
        return $codexPython
    }

    throw "python.exe was not found."
}

function Get-SymbolAddress {
    param([string]$Nm, [string]$Elf, [string]$Symbol)
    $line = & $Nm -n $Elf | Select-String -Pattern "\s$Symbol$" | Select-Object -First 1
    if (-not $line) {
        throw "Symbol was not found in ELF: $Symbol"
    }

    $hex = ($line.ToString().Trim() -split "\s+")[0]
    return [Convert]::ToUInt32($hex, 16)
}

function Read-UInt32 {
    param([string]$Programmer, [uint32]$Address, [string]$TempPath)
    & $Programmer -c port=SWD mode=HotPlug freq=1000 -u ("0x{0:X8}" -f $Address) 4 $TempPath | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to read target memory at 0x$($Address.ToString('X8'))"
    }
    $bytes = [System.IO.File]::ReadAllBytes($TempPath)
    return [BitConverter]::ToUInt32($bytes, 0)
}

Assert-File $ElfPath "Missing ELF file. Build the $Config preset first"
New-Item -ItemType Directory -Force -Path $RawDir | Out-Null

$Programmer = Find-Programmer
$Nm = Find-Nm
$Python = Find-Python

$SessionCountAddress = Get-SymbolAddress $Nm $ElfPath "g_walk_metrics_debug_session_count"
$SessionsAddress = Get-SymbolAddress $Nm $ElfPath "g_walk_metrics_debug_sessions"
$ControlAddress = Get-SymbolAddress $Nm $ElfPath "g_walk_metrics_debug_export_control"
$MotionLogClearAddress = Get-SymbolAddress $Nm $ElfPath "g_motion_log_clear_control"
$ExportBufferAddress = Get-SymbolAddress $Nm $ElfPath "g_walk_metrics_debug_export_buffer"
$ControlTemp = Join-Path $RawDir ".walk_export_control.bin"
$ChunkTemp = Join-Path $RawDir ".walk_export_chunk.bin"
$ValueTemp = Join-Path $RawDir ".walk_export_value.bin"
$SessionTemp = Join-Path $RawDir ".walk_export_sessions.bin"
$RecordBytes = 16
$SessionRecordBytes = 20
$ExportBufferBytes = 32768

if ($EraseStoredData) {
    $RequestId = [Convert]::ToUInt32("C1EA0001", 16)
    & $Programmer -c port=SWD mode=HotPlug freq=1000 `
        -w32 ("0x{0:X8}" -f $ControlAddress) `
        "0xFFFFFFFF" "0x00000000" ("0x{0:X8}" -f $RequestId) | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to submit persistent motion-data erase request"
    }

    $RawCleared = $false
    for ($Poll = 0; $Poll -lt 300; $Poll++) {
        Start-Sleep -Milliseconds 50
        & $Programmer -c port=SWD mode=HotPlug freq=1000 `
            -u ("0x{0:X8}" -f $ControlAddress) 20 $ControlTemp | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to poll persistent motion-data erase request"
        }
        $Control = [System.IO.File]::ReadAllBytes($ControlTemp)
        $CompletedId = [BitConverter]::ToUInt32($Control, 12)
        $Status = [BitConverter]::ToUInt32($Control, 16)
        if ($CompletedId -eq $RequestId) {
            if ($Status -ne 2) {
                throw "Firmware rejected persistent motion-data erase request (status=$Status)"
            }
            $RawCleared = $true
            break
        }
    }
    if (-not $RawCleared) {
        throw "Timed out waiting for persistent motion-data erase request"
    }

    $MotionRequestId = [Convert]::ToUInt32("C1EA0002", 16)
    & $Programmer -c port=SWD mode=HotPlug freq=1000 `
        -w32 ("0x{0:X8}" -f $MotionLogClearAddress) `
        ("0x{0:X8}" -f $MotionRequestId) | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Raw sessions were cleared, but the motion-summary clear request could not be submitted"
    }

    $SummaryCleared = $false
    for ($Poll = 0; $Poll -lt 300; $Poll++) {
        Start-Sleep -Milliseconds 50
        & $Programmer -c port=SWD mode=HotPlug freq=1000 `
            -u ("0x{0:X8}" -f $MotionLogClearAddress) 12 $ControlTemp | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to poll the motion-summary clear request"
        }
        $Control = [System.IO.File]::ReadAllBytes($ControlTemp)
        $CompletedId = [BitConverter]::ToUInt32($Control, 4)
        $Status = [BitConverter]::ToUInt32($Control, 8)
        if ($CompletedId -eq $MotionRequestId) {
            if ($Status -ne 2) {
                throw "Firmware rejected the motion-summary clear request (status=$Status)"
            }
            $SummaryCleared = $true
            break
        }
    }
    Remove-Item -LiteralPath $ControlTemp -Force -ErrorAction SilentlyContinue
    if (-not $SummaryCleared) {
        throw "Timed out waiting for the motion-summary clear request"
    }

    Write-Host "Persistent raw motion sessions and EEPROM motion summaries cleared."
    Write-Host "Raw data sectors will be erased lazily when reused."
    exit 0
}

$SessionCount = Read-UInt32 $Programmer $SessionCountAddress $ValueTemp
if ($SessionCount -gt 64) {
    throw "Invalid session count: $SessionCount"
}
if ($SessionCount -eq 0) {
    throw "No motion sessions are available. Record a session before exporting."
}

$SessionBytes = [uint32]($SessionCount * $SessionRecordBytes)
& $Programmer -c port=SWD mode=HotPlug freq=1000 `
    -u ("0x{0:X8}" -f $SessionsAddress) $SessionBytes $SessionTemp | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "Failed to read motion session directory"
}
$SessionData = [System.IO.File]::ReadAllBytes($SessionTemp)
$Sessions = @()
for ($Index = 0; $Index -lt $SessionCount; $Index++) {
    $Offset = $Index * $SessionRecordBytes
    $StatusValue = [BitConverter]::ToUInt32($SessionData, $Offset + 16)
    $Sessions += [pscustomobject]@{
        Index = $Index
        FromLatest = $SessionCount - $Index - 1
        StartIndex = [BitConverter]::ToUInt32($SessionData, $Offset)
        Samples = [BitConverter]::ToUInt32($SessionData, $Offset + 4)
        DurationSeconds = [math]::Round([BitConverter]::ToUInt32($SessionData, $Offset + 4) / 50.0, 2)
        StartTick = [BitConverter]::ToUInt32($SessionData, $Offset + 8)
        EndTick = [BitConverter]::ToUInt32($SessionData, $Offset + 12)
        Status = if ($StatusValue -eq 2) { "complete" } elseif ($StatusValue -eq 1) { "active" } else { "unknown" }
    }
}

if ($ListSessions) {
    $Sessions | Format-Table Index, FromLatest, Samples, DurationSeconds, Status, StartIndex -AutoSize
    Remove-Item -LiteralPath $SessionTemp, $ValueTemp -Force -ErrorAction SilentlyContinue
    exit 0
}
if ($SessionFromLatest -ge $SessionCount) {
    throw "SessionFromLatest=$SessionFromLatest is out of range; only $SessionCount sessions exist"
}

$SelectedSession = $Sessions[$SessionCount - $SessionFromLatest - 1]
$StartIndex = [uint32]$SelectedSession.StartIndex
$Count = [uint32]$SelectedSession.Samples
$WriteIndex = [uint32]($StartIndex + $Count)
if (($Count -eq 0) -or ($StartIndex -ge $Capacity) -or ($WriteIndex -gt $Capacity)) {
    throw "Invalid selected session: start=$StartIndex count=$Count capacity=$Capacity"
}

Write-Host "Selected session index $($SelectedSession.Index) ($SessionFromLatest from latest, status=$($SelectedSession.Status))"

$MetaBytes = New-Object byte[] 8
[BitConverter]::GetBytes([uint32]$WriteIndex).CopyTo($MetaBytes, 0)
[BitConverter]::GetBytes([uint32]$Count).CopyTo($MetaBytes, 4)
[System.IO.File]::WriteAllBytes($MetaBin, $MetaBytes)

$Remaining = [uint32]$Count
$CurrentIndex = [uint32]$StartIndex
& $Programmer -c port=SWD mode=HotPlug freq=1000 `
    -u ("0x{0:X8}" -f $ControlAddress) 20 $ControlTemp | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "Failed to read the current Flash export request state"
}
$Control = [System.IO.File]::ReadAllBytes($ControlTemp)
$CompletedId = [BitConverter]::ToUInt32($Control, 12)
if ($CompletedId -eq [uint32]::MaxValue) {
    $RequestId = [uint32]1
}
else {
    $RequestId = [uint32]($CompletedId + 1)
}

$ClearRequestId = [Convert]::ToUInt32("C1EA0001", 16)
if ($RequestId -eq $ClearRequestId) {
    $RequestId = [uint32]($RequestId + 1)
}
$Stream = [System.IO.File]::Open($SamplesBin, [System.IO.FileMode]::Create,
                                [System.IO.FileAccess]::Write,
                                [System.IO.FileShare]::None)
try {
    Write-Host "Exporting $Count persistent Flash samples ($([math]::Round($Count / 50.0, 2)) seconds)"
    while ($Remaining -gt 0) {
        $UntilWrap = $Capacity - $CurrentIndex
        $ChunkRecords = [math]::Min([math]::Min($Remaining, $UntilWrap),
                                    [int]($ExportBufferBytes / $RecordBytes))
        $PsramAddress = [uint32]($CurrentIndex * $RecordBytes)
        $ChunkBytes = [uint32]($ChunkRecords * $RecordBytes)

        & $Programmer -c port=SWD mode=HotPlug freq=1000 `
            -w32 ("0x{0:X8}" -f $ControlAddress) `
            ("0x{0:X8}" -f $PsramAddress) `
            ("0x{0:X8}" -f $ChunkBytes) `
            ("0x{0:X8}" -f $RequestId) | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to submit Flash export request $RequestId"
        }

        $Ready = $false
        for ($Poll = 0; $Poll -lt 300; $Poll++) {
            Start-Sleep -Milliseconds 50
            & $Programmer -c port=SWD mode=HotPlug freq=1000 `
                -u ("0x{0:X8}" -f $ControlAddress) 20 $ControlTemp | Out-Null
            if ($LASTEXITCODE -ne 0) {
                throw "Failed to poll Flash export request $RequestId"
            }
            $Control = [System.IO.File]::ReadAllBytes($ControlTemp)
            $CompletedId = [BitConverter]::ToUInt32($Control, 12)
            $Status = [BitConverter]::ToUInt32($Control, 16)
            if ($CompletedId -eq $RequestId) {
                if ($Status -ne 2) {
                    throw "Firmware rejected Flash export request $RequestId (status=$Status)"
                }
                $Ready = $true
                break
            }
        }
        if (-not $Ready) {
            throw "Timed out waiting for Flash export request $RequestId"
        }

        & $Programmer -c port=SWD mode=HotPlug freq=1000 `
            -u ("0x{0:X8}" -f $ExportBufferAddress) $ChunkBytes $ChunkTemp | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to read Flash export buffer for request $RequestId"
        }
        $Chunk = [System.IO.File]::ReadAllBytes($ChunkTemp)
        $Stream.Write($Chunk, 0, $ChunkBytes)

        $Remaining -= $ChunkRecords
        $CurrentIndex = ($CurrentIndex + $ChunkRecords) % $Capacity
        $RequestId++
        $Done = $Count - $Remaining
        Write-Progress -Activity "Exporting persistent Flash motion data" `
            -Status "$Done / $Count samples" -PercentComplete (($Done * 100.0) / $Count)
    }
}
finally {
    $Stream.Dispose()
    Write-Progress -Activity "Exporting persistent Flash motion data" -Completed
    Remove-Item -LiteralPath $ControlTemp, $ChunkTemp, $ValueTemp, $SessionTemp -Force -ErrorAction SilentlyContinue
}

$AutoRouted = -not [bool]$OutDir
if ($AutoRouted) {
    $ResolvedCategory = Resolve-DatasetCategory $Category $Label
    $Desktop = [Environment]::GetFolderPath("Desktop")
    $TrainingCategoryDir = Join-Path $Desktop "raw_data\$ResolvedCategory"
    $TestCategoryDir = Join-Path $Desktop "test_data\$ResolvedCategory"
    New-Item -ItemType Directory -Force -Path $TrainingCategoryDir, $TestCategoryDir | Out-Null

    $TrainingCount = Get-TrainingDatasetCount $TrainingCategoryDir
    if ($TrainingCount -lt $TrainingLimit) {
        $DestinationCategoryDir = $TrainingCategoryDir
        $DatasetRole = "training"
    } else {
        $DestinationCategoryDir = $TestCategoryDir
        $DatasetRole = "test"
    }

    $ConversionOutDir = Get-UniqueDirectoryPath $DestinationCategoryDir "NanoEdge_$Label"
    Write-Host "Dataset routing: category=$ResolvedCategory role=$DatasetRole training=$TrainingCount/$TrainingLimit"
    Write-Host "Dataset package: $ConversionOutDir"
} elseif ([System.IO.Path]::IsPathRooted($OutDir)) {
    $ConversionOutDir = $OutDir
} else {
    $ConversionOutDir = Join-Path $Root $OutDir
}

& $Python (Join-Path $PSScriptRoot "convert_walk_debug_to_neai.py") `
    --samples-bin $SamplesBin `
    --meta-bin $MetaBin `
    --label $Label `
    --out-dir $ConversionOutDir `
    --capacity $Capacity `
    --window $Window `
    --stride $Stride `
    --linear
if ($LASTEXITCODE -ne 0) {
    throw "CSV conversion failed"
}

Copy-Item -LiteralPath $SamplesBin -Destination (Join-Path $ConversionOutDir "$Label`_samples.bin") -Force
Copy-Item -LiteralPath $MetaBin -Destination (Join-Path $ConversionOutDir "$Label`_meta.bin") -Force
