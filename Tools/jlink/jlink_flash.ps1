[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [string]$Firmware,
    [string]$Device = "PY32F032x8",
    [ValidateSet("SWD", "JTAG")]
    [string]$Interface = "SWD",
    [ValidateRange(1, 50000)]
    [int]$Speed = 10000,
    [string]$Address = "0x08000000",
    [string]$SerialNumber,
    [string]$JLinkExe,
    [switch]$EraseAll,
    [switch]$SkipErase,
    [switch]$NoReset
)

$ErrorActionPreference = "Stop"

function Set-FlashProgress {
    param(
        [int]$Percent,
        [string]$Status,
        [switch]$Completed
    )

    $barWidth = 30
    if ($Completed) {
        $bar = "#" * $barWidth
        $progressText = "`r[FLASH] [{0}] 100% {1}" -f $bar, $Status
    }
    else {
        $Percent = [Math]::Max(1, [Math]::Min(99, $Percent))
        $completedWidth = [Math]::Floor($Percent * $barWidth / 100)
        $bar = ("#" * $completedWidth).PadRight($barWidth, "-")
        $progressText = "`r[FLASH] [{0}] {1,3}% {2}" -f $bar, $Percent, $Status
    }
    Write-Host $progressText.PadRight(80) -NoNewline
}

function Get-FlashEstimatedSeconds {
    param(
        [long]$FileSize,
        [int]$SwdSpeedKHz,
        [bool]$EraseEnabled
    )

    # Allow for SWD protocol overhead, target flash programming time and a verify pass.
    $rawBytesPerSecond = [Math]::Max(1.0, $SwdSpeedKHz * 1000.0 / 8.0)
    $programBytesPerSecond = [Math]::Max(1.0, $rawBytesPerSecond * 0.20)
    $verifyBytesPerSecond = [Math]::Max(1.0, $rawBytesPerSecond * 0.45)
    $fixedSeconds = if ($EraseEnabled) { 2.0 } else { 0.8 }

    return [Math]::Max(
        1.0,
        $fixedSeconds + ($FileSize / $programBytesPerSecond) +
            ($FileSize / $verifyBytesPerSecond)
    )
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

try {
    if ($EraseAll -and $SkipErase) {
        throw "-EraseAll and -SkipErase cannot be used together."
    }

    $repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
    if (-not $Firmware) {
        $Firmware = Join-Path $repoRoot "Project\Output\D940v2_firmware.bin"
    }
    if (-not (Test-Path -LiteralPath $Firmware -PathType Leaf)) {
        throw "Firmware file does not exist: $Firmware"
    }
    $firmwarePath = (Resolve-Path -LiteralPath $Firmware).Path

    $addressValue = 0L
    $addressText = $Address.Trim()
    if ($addressText -match '^0[xX][0-9a-fA-F]+$') {
        $addressValue = [Convert]::ToInt64($addressText.Substring(2), 16)
    }
    elseif (-not [Int64]::TryParse($addressText, [ref]$addressValue)) {
        throw "Invalid flash address: $Address"
    }
    if ($addressValue -lt 0 -or $addressValue -gt [UInt32]::MaxValue) {
        throw "Flash address is outside the 32-bit address range: $Address"
    }
    $normalizedAddress = "0x{0:X8}" -f $addressValue

    $jlinkPath = Resolve-JLinkExecutable $JLinkExe
    $commandFile = Join-Path ([IO.Path]::GetTempPath()) ("jlink_flash_{0}.jlink" -f [Guid]::NewGuid().ToString("N"))
    $commands = @("ExitOnError 1", "r", "h")
    if (-not $SkipErase) { $commands += "erase" }
    $commands += "loadbin `"$firmwarePath`", $normalizedAddress"
    $commands += "verifybin `"$firmwarePath`", $normalizedAddress"
    if (-not $NoReset) { $commands += @("r", "g") }
    $commands += "q"
    [IO.File]::WriteAllLines($commandFile, $commands, [Text.Encoding]::ASCII)

    Write-Host "[INFO] J-Link   : $jlinkPath"
    Write-Host "[INFO] Device   : $Device"
    Write-Host "[INFO] Interface: $Interface @ $Speed kHz"
    Write-Host "[INFO] Firmware : $firmwarePath"
    Write-Host "[INFO] Address  : $normalizedAddress"

    $firmwareSize = (Get-Item -LiteralPath $firmwarePath).Length
    $estimatedSeconds = Get-FlashEstimatedSeconds `
        -FileSize $firmwareSize `
        -SwdSpeedKHz $Speed `
        -EraseEnabled (-not $SkipErase)
    Write-Host ("[INFO] Estimate : {0:N1} seconds ({1:N0} bytes)" -f `
        $estimatedSeconds, $firmwareSize)

    $jlinkArgs = @("-NoGui", "1", "-Device", $Device, "-If", $Interface,
        "-Speed", $Speed, "-AutoConnect", "1", "-CommanderScript", $commandFile)
    if ($SerialNumber) {
        $jlinkArgs = @("-SelectEmuBySN", $SerialNumber) + $jlinkArgs
    }

    $jlinkOutput = [System.Collections.Generic.List[string]]::new()
    $jobInput = @{
        Executable = $jlinkPath
        Arguments = [string[]]$jlinkArgs
    }
    $flashJob = Start-Job -ArgumentList $jobInput -ScriptBlock {
        param($InputData)
        $arguments = $InputData.Arguments
        & $InputData.Executable @arguments 2>&1 |
            ForEach-Object { $_.ToString() }
        "__JLINK_EXIT_CODE__=$LASTEXITCODE"
    }

    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    while ($flashJob.State -in @("NotStarted", "Running")) {
        Receive-Job -Job $flashJob | ForEach-Object { $jlinkOutput.Add($_.ToString()) }
        $percent = [Math]::Min(
            99,
            [Math]::Max(1, [Math]::Floor($stopwatch.Elapsed.TotalSeconds * 98 / $estimatedSeconds) + 1)
        )
        Set-FlashProgress $percent "Programming and verifying"
        Start-Sleep -Milliseconds 100
    }
    $stopwatch.Stop()
    Receive-Job -Job $flashJob | ForEach-Object { $jlinkOutput.Add($_.ToString()) }
    Remove-Job -Job $flashJob -Force

    $exitCodeLine = $jlinkOutput | Where-Object { $_ -like "__JLINK_EXIT_CODE__=*" } |
        Select-Object -Last 1
    $exitCode = if ($exitCodeLine) {
        [int]($exitCodeLine -replace '^__JLINK_EXIT_CODE__=', '')
    }
    else {
        -1
    }
    [void]$jlinkOutput.Remove($exitCodeLine)

    $failurePattern = '(?i)(cannot connect|could not find.*emulator|failed to open|verification failed|verify failed|script processing failure|error while programming)'
    if ($exitCode -ne 0 -or (($jlinkOutput -join "`n") -match $failurePattern)) {
        Write-Host ""
        $jlinkOutput | ForEach-Object { Write-Host $_ }
        throw "J-Link programming failed (exit code: $exitCode)."
    }

    Set-FlashProgress 0 "Completed" -Completed
    Write-Host ""
    Write-Host "[OK] Programming and verification completed successfully." -ForegroundColor Green
    exit 0
}
catch {
    Write-Error $_.Exception.Message
    exit 1
}
finally {
    if ($commandFile -and (Test-Path -LiteralPath $commandFile)) {
        Remove-Item -LiteralPath $commandFile -Force -ErrorAction SilentlyContinue
    }
}
