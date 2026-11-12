<#
.SYNOPSIS
Reads a Flash region through J-Link and compares it byte-for-byte with a BIN file.

.EXAMPLE
.\compare_flash.ps1 -Region App

.EXAMPLE
.\compare_flash.ps1 -BinFile .\image.bin -Address 0x08004000 -Length 0x2000

.EXAMPLE
.\compare_flash.ps1 -BinFile .\firmware.bin -BinOffset 0x4000 `
    -Address 0x08004000 -Length 0x2000 -ReadbackFile .\flash_dump.bin

.NOTES
Exit code 0 means match, 2 means data mismatch, and 1 means an execution error.
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [string]$BinFile,
    [ValidateSet("App", "Boot", "Full", "Custom")]
    [string]$Region,
    [string]$Address,
    [long]$Length = 0,
    [ValidateRange(0, [long]::MaxValue)]
    [long]$BinOffset = 0,
    [string]$Device = "PY32F032x8",
    [ValidateSet("SWD", "JTAG")]
    [string]$Interface = "SWD",
    [ValidateRange(1, 50000)]
    [int]$Speed = 5000,
    [string]$SerialNumber,
    [string]$JLinkExe,
    [string]$FlashLoader,
    [string]$ReadbackFile,
    [ValidateRange(1, 1000)]
    [int]$MaxDifferences = 16,
    [switch]$KeepReadback,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

function ConvertTo-UInt32Value {
    param([string]$Value, [string]$Name)

    if ([string]::IsNullOrWhiteSpace($Value)) {
        throw "$Name cannot be empty."
    }

    $number = 0L
    $text = $Value.Trim()
    if ($text -match '^0[xX][0-9a-fA-F]+$') {
        $number = [Convert]::ToInt64($text.Substring(2), 16)
    }
    elseif (-not [long]::TryParse($text, [ref]$number)) {
        throw "Invalid ${Name}: $Value"
    }

    if ($number -lt 0 -or $number -gt [uint32]::MaxValue) {
        throw "$Name is outside the 32-bit range: $Value"
    }
    return $number
}

function Resolve-JLinkExecutable {
    param([string]$ExplicitPath)

    if ($ExplicitPath) {
        if (-not (Test-Path -LiteralPath $ExplicitPath -PathType Leaf)) {
            throw "J-Link executable does not exist: $ExplicitPath"
        }
        return (Resolve-Path -LiteralPath $ExplicitPath).Path
    }

    $command = Get-Command "JLink.exe" -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    $searchRoots = @(
        (Join-Path $env:ProgramFiles "SEGGER"),
        "C:\Keil_v5\ARM\Segger",
        "D:\Keil_v5\ARM\Segger",
        "C:\Keil\ARM\Segger",
        "D:\Keil\ARM\Segger"
    )
    if (${env:ProgramFiles(x86)}) {
        $searchRoots += Join-Path ${env:ProgramFiles(x86)} "SEGGER"
    }
    if ($env:KEIL_ROOT) {
        $searchRoots += Join-Path $env:KEIL_ROOT "ARM\Segger"
    }

    $found = foreach ($root in $searchRoots) {
        if (Test-Path -LiteralPath $root -PathType Container) {
            Get-ChildItem -LiteralPath $root -Filter "JLink.exe" -File -Recurse -ErrorAction SilentlyContinue
        }
    }
    $latest = $found | Sort-Object FullName -Descending | Select-Object -First 1
    if ($latest) { return $latest.FullName }

    throw "JLink.exe was not found. Install SEGGER J-Link Software or use -JLinkExe <path>."
}

function Resolve-PY32F032FlashLoader {
    param([string]$ExplicitPath)

    if ($ExplicitPath) {
        if (-not (Test-Path -LiteralPath $ExplicitPath -PathType Leaf)) {
            throw "PY32F032 flash loader does not exist: $ExplicitPath"
        }
        return (Resolve-Path -LiteralPath $ExplicitPath).Path
    }

    $candidates = [System.Collections.Generic.List[IO.FileInfo]]::new()
    if ($env:LOCALAPPDATA) {
        Get-Item -Path (Join-Path $env:LOCALAPPDATA `
            "Arm\Packs\Puya\PY32F0xx_DFP\*\CMSIS\Flash\PY106xx_64.FLM") `
            -ErrorAction SilentlyContinue | ForEach-Object { $candidates.Add($_) }
    }
    Get-PSDrive -PSProvider FileSystem | ForEach-Object {
        $pattern = Join-Path $_.Root `
            "Users\*\AppData\Local\Arm\Packs\Puya\PY32F0xx_DFP\*\CMSIS\Flash\PY106xx_64.FLM"
        Get-Item -Path $pattern -ErrorAction SilentlyContinue |
            ForEach-Object { $candidates.Add($_) }
    }

    $loader = $candidates | Sort-Object FullName -Descending | Select-Object -First 1
    if ($loader) { return $loader.FullName }
    throw "PY32F032 flash loader PY106xx_64.FLM was not found. Use -FlashLoader <path>."
}

function Get-Crc32 {
    param([byte[]]$Data)

    # Windows PowerShell 5.1 treats hexadecimal literals with bit 31 set as
    # signed Int32 values. Use decimal UInt32 constants to avoid converting
    # 0xFFFFFFFF (-1 as Int32) to UInt32.
    [uint32]$allOnes = 4294967295
    [uint32]$polynomial = 3988292384 # 0xEDB88320
    [uint32]$crc = $allOnes
    foreach ($value in $Data) {
        $crc = $crc -bxor [uint32]$value
        for ($bit = 0; $bit -lt 8; $bit++) {
            if (($crc -band 1) -ne 0) {
                $crc = ($crc -shr 1) -bxor $polynomial
            }
            else {
                $crc = $crc -shr 1
            }
        }
    }
    return [uint32]($crc -bxor $allOnes)
}

try {
    $repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

    if (-not $Region -and -not $BinFile) {
        Write-Host "Select the Flash region to compare:"
        Write-Host "  1. APP        (0x08004000)"
        Write-Host "  2. Bootloader (0x08000000)"
        Write-Host "  3. Full Flash (0x08000000)"
        Write-Host "  4. Custom"
        $selection = Read-Host "Selection [1]"
        if ([string]::IsNullOrWhiteSpace($selection)) { $selection = "1" }
        $Region = switch ($selection) {
            "1" { "App" }
            "2" { "Boot" }
            "3" { "Full" }
            "4" { "Custom" }
            default { throw "Invalid region selection: $selection" }
        }
    }

    switch ($Region) {
        "App" {
            if (-not $BinFile) { $BinFile = Join-Path $repoRoot "Projects\Output\ba_fan_app.bin" }
            if (-not $Address) { $Address = "0x08004000" }
        }
        "Boot" {
            if (-not $BinFile) { $BinFile = Join-Path $repoRoot "Projects\Output\ba_fan_boot.bin" }
            if (-not $Address) { $Address = "0x08000000" }
        }
        "Full" {
            if (-not $BinFile) { $BinFile = Join-Path $repoRoot "Projects\Output\ba_fan_firmware.bin" }
            if (-not $Address) { $Address = "0x08000000" }
        }
        "Custom" {
            if (-not $BinFile) { $BinFile = Read-Host "BIN file path" }
            if (-not $Address) { $Address = Read-Host "Flash start address (for example 0x08004000)" }
        }
        default {
            if (-not $Address) { $Address = "0x08000000" }
        }
    }

    if (-not (Test-Path -LiteralPath $BinFile -PathType Leaf)) {
        throw "BIN file does not exist: $BinFile"
    }
    $binPath = (Resolve-Path -LiteralPath $BinFile).Path
    [byte[]]$allBinBytes = [IO.File]::ReadAllBytes($binPath)
    if ($BinOffset -ge $allBinBytes.LongLength) {
        throw "BIN offset $BinOffset is outside the file ($($allBinBytes.LongLength) bytes)."
    }
    if ($Length -eq 0) { $Length = $allBinBytes.LongLength - $BinOffset }
    if ($Length -le 0) { throw "Comparison length must be greater than zero." }
    if ($BinOffset + $Length -gt $allBinBytes.LongLength) {
        throw "BIN offset + length exceeds the file size."
    }
    if ($Length -gt [int]::MaxValue) {
        throw "Comparison length exceeds the maximum supported byte-array size."
    }

    $startAddress = ConvertTo-UInt32Value $Address "Flash address"
    if ($startAddress + $Length - 1 -gt [uint32]::MaxValue) {
        throw "Selected Flash region exceeds the 32-bit address range."
    }
    $addressText = "0x{0:X8}" -f $startAddress
    $lengthText = "0x{0:X}" -f $Length

    [byte[]]$binBytes = New-Object byte[] ([int]$Length)
    [Array]::Copy($allBinBytes, $BinOffset, $binBytes, 0, $Length)

    $jlinkPath = Resolve-JLinkExecutable $JLinkExe
    $flashLoaderPath = Resolve-PY32F032FlashLoader $FlashLoader
    $deviceSupportDir = Join-Path ([IO.Path]::GetTempPath()) `
        ("jlink_py32f032_compare_{0}" -f [Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $deviceSupportDir -Force | Out-Null
    Copy-Item -LiteralPath $flashLoaderPath `
        -Destination (Join-Path $deviceSupportDir "PY106xx_64.FLM") -Force

    $deviceXml = @"
<DataBase>
  <Device>
    <ChipInfo Vendor="Puya" Name="PY32F032x8" WorkRAMAddr="0x20000000" WorkRAMSize="0x1000" Core="JLINK_CORE_CORTEX_M0"/>
    <FlashBankInfo Name="Main Flash" BaseAddr="0x08000000" MaxSize="0x10000" Loader="PY106xx_64.FLM" LoaderType="FLASH_ALGO_TYPE_CMSIS" AlwaysPresent="1"/>
  </Device>
</DataBase>
"@
    [IO.File]::WriteAllText(
        (Join-Path $deviceSupportDir "JLinkDevices.xml"),
        $deviceXml,
        [Text.UTF8Encoding]::new($false)
    )

    if ($ReadbackFile) {
        $readbackPath = [IO.Path]::GetFullPath($ReadbackFile)
        $readbackDirectory = Split-Path -Parent $readbackPath
        if ($readbackDirectory -and -not (Test-Path -LiteralPath $readbackDirectory)) {
            New-Item -ItemType Directory -Path $readbackDirectory -Force | Out-Null
        }
        $KeepReadback = $true
    }
    else {
        $readbackPath = Join-Path ([IO.Path]::GetTempPath()) `
            ("jlink_readback_{0}.bin" -f [Guid]::NewGuid().ToString("N"))
    }

    $commandFile = Join-Path ([IO.Path]::GetTempPath()) `
        ("jlink_compare_{0}.jlink" -f [Guid]::NewGuid().ToString("N"))
    $commands = @(
        "ExitOnError 1",
        "exec JLinkDevicesXMLPath = $deviceSupportDir",
        "device $Device",
        "r",
        "h",
        "le",
        "savebin `"$readbackPath`", $addressText, $lengthText",
        "q"
    )
    [IO.File]::WriteAllLines($commandFile, $commands, [Text.Encoding]::ASCII)

    Write-Host "[INFO] BIN       : $binPath"
    Write-Host "[INFO] BIN offset: 0x$($BinOffset.ToString('X'))"
    Write-Host "[INFO] Flash     : $addressText - 0x$(($startAddress + $Length - 1).ToString('X8'))"
    Write-Host "[INFO] Length    : $Length bytes ($lengthText)"
    Write-Host "[INFO] J-Link    : $jlinkPath"
    Write-Host "[INFO] Target    : $Device, $Interface @ $Speed kHz"

    if ($DryRun) {
        Write-Host "[INFO] Dry run command file:" -ForegroundColor Yellow
        $commands | ForEach-Object { Write-Host "       $_" }
        exit 0
    }

    $jlinkArgs = @("-NoGui", "1", "-If", $Interface, "-Speed", $Speed,
        "-AutoConnect", "0", "-CommanderScript", $commandFile)
    if ($SerialNumber) {
        $jlinkArgs = @("-SelectEmuBySN", $SerialNumber) + $jlinkArgs
    }

    $jlinkOutput = @(& $jlinkPath @jlinkArgs 2>&1 | ForEach-Object { $_.ToString() })
    $exitCode = $LASTEXITCODE
    $failurePattern = '(?i)(cannot connect|could not find.*emulator|failed to open|script processing failure|error while|\berror:)'
    if ($exitCode -ne 0 -or (($jlinkOutput -join "`n") -match $failurePattern)) {
        $jlinkOutput | ForEach-Object { Write-Host $_ }
        throw "J-Link read failed (exit code: $exitCode)."
    }
    if (-not (Test-Path -LiteralPath $readbackPath -PathType Leaf)) {
        throw "J-Link did not create the readback file."
    }

    [byte[]]$flashBytes = [IO.File]::ReadAllBytes($readbackPath)
    if ($flashBytes.LongLength -ne $Length) {
        throw "Readback size mismatch: expected $Length, received $($flashBytes.LongLength) bytes."
    }

    $differenceCount = 0L
    $shownDifferences = [System.Collections.Generic.List[string]]::new()
    for ($index = 0; $index -lt $flashBytes.Length; $index++) {
        if ($flashBytes[$index] -ne $binBytes[$index]) {
            $differenceCount++
            if ($shownDifferences.Count -lt $MaxDifferences) {
                $differenceAddress = $startAddress + $index
                $differenceBinOffset = $BinOffset + $index
                $shownDifferences.Add((
                    "  Flash 0x{0:X8} / BIN 0x{1:X}: flash=0x{2:X2}, bin=0x{3:X2}" -f `
                    $differenceAddress, $differenceBinOffset, $flashBytes[$index], $binBytes[$index]
                ))
            }
        }
    }

    $flashCrc = Get-Crc32 $flashBytes
    $binCrc = Get-Crc32 $binBytes
    Write-Host ("[INFO] Flash CRC32: 0x{0:X8}" -f $flashCrc)
    Write-Host ("[INFO] BIN CRC32  : 0x{0:X8}" -f $binCrc)
    if ($KeepReadback) { Write-Host "[INFO] Readback   : $readbackPath" }

    if ($differenceCount -eq 0) {
        Write-Host "[PASS] Flash region matches the BIN file." -ForegroundColor Green
        exit 0
    }

    Write-Host "[FAIL] Flash region does not match the BIN file." -ForegroundColor Red
    Write-Host "[INFO] Different bytes: $differenceCount / $Length"
    $shownDifferences | ForEach-Object { Write-Host $_ }
    if ($differenceCount -gt $shownDifferences.Count) {
        Write-Host "  ... $($differenceCount - $shownDifferences.Count) additional differences not shown"
    }
    exit 2
}
catch {
    Write-Error $_.Exception.Message
    exit 1
}
finally {
    if ($commandFile -and (Test-Path -LiteralPath $commandFile)) {
        Remove-Item -LiteralPath $commandFile -Force -ErrorAction SilentlyContinue
    }
    if (-not $KeepReadback -and $readbackPath -and (Test-Path -LiteralPath $readbackPath)) {
        Remove-Item -LiteralPath $readbackPath -Force -ErrorAction SilentlyContinue
    }
    if ($deviceSupportDir -and (Test-Path -LiteralPath $deviceSupportDir -PathType Container)) {
        $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
        $supportPath = [IO.Path]::GetFullPath($deviceSupportDir)
        if ($supportPath.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) {
            Remove-Item -LiteralPath $supportPath -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}
