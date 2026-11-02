[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [string]$Firmware,
    [string]$Device = "PY32F032x8",
    [ValidateSet("SWD", "JTAG")]
    [string]$Interface = "SWD",
    [ValidateRange(1, 50000)]
    [int]$Speed = 5000,
    [string]$Address = "0x08000000",
    [string]$RegionStart,
    [string]$RegionEnd,
    [string]$SerialNumber,
    [string]$JLinkExe,
    [string]$FlashLoader,
    [string]$LogFile,
    [switch]$EraseOnly,
    [switch]$ResetOnly,
    [switch]$EraseAll,
    [switch]$SkipErase,
    [switch]$NoReset,
    [switch]$DryRun
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

function ConvertTo-FlashAddress {
    param(
        [string]$Value,
        [string]$Name
    )

    $addressValue = 0L
    $addressText = $Value.Trim()
    if ($addressText -match '^0[xX][0-9a-fA-F]+$') {
        $addressValue = [Convert]::ToInt64($addressText.Substring(2), 16)
    }
    elseif (-not [Int64]::TryParse($addressText, [ref]$addressValue)) {
        throw "Invalid ${Name}: $Value"
    }
    if ($addressValue -lt 0 -or $addressValue -gt [UInt32]::MaxValue) {
        throw "${Name} is outside the 32-bit address range: $Value"
    }

    return @{
        Value = $addressValue
        Text = "0x{0:X8}" -f $addressValue
    }
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

    throw "PY32F032 flash loader PY106xx_64.FLM was not found. Install the Puya PY32F0xx DFP or use -FlashLoader <path>."
}

try {
    if ($EraseOnly -and $ResetOnly) {
        throw "-EraseOnly and -ResetOnly cannot be used together."
    }
    if ($EraseAll -and $SkipErase) {
        throw "-EraseAll and -SkipErase cannot be used together."
    }
    if (($RegionStart -and -not $RegionEnd) -or ($RegionEnd -and -not $RegionStart)) {
        throw "-RegionStart and -RegionEnd must be specified together."
    }
    if (($RegionStart -or $RegionEnd) -and ($EraseAll -or $SkipErase)) {
        throw "A bounded flash region cannot be combined with -EraseAll or -SkipErase."
    }
    if (($EraseOnly -or $ResetOnly) -and
        ($Firmware -or $RegionStart -or $RegionEnd -or $EraseAll -or $SkipErase)) {
        throw "Erase/reset-only operations cannot include firmware programming options."
    }

    $repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
    if (-not $LogFile) {
        $LogFile = Join-Path $repoRoot "Projects\Output\jlink_flash.log"
    }
    $logDirectory = Split-Path -Parent $LogFile
    if ($logDirectory -and -not (Test-Path -LiteralPath $logDirectory -PathType Container)) {
        New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
    }
    $logPath = [IO.Path]::GetFullPath($LogFile)

    $firmwarePath = $null
    $firmwareSize = 0L
    $flashAddress = $null
    $normalizedAddress = $null
    if (-not $EraseOnly -and -not $ResetOnly) {
        if (-not $Firmware) {
            $Firmware = Join-Path $repoRoot "Projects\Output\ba_fan_firmware.bin"
        }
        if (-not (Test-Path -LiteralPath $Firmware -PathType Leaf)) {
            throw "Firmware file does not exist: $Firmware"
        }
        $firmwarePath = (Resolve-Path -LiteralPath $Firmware).Path
        $firmwareSize = (Get-Item -LiteralPath $firmwarePath).Length
        $flashAddress = ConvertTo-FlashAddress $Address "flash address"
        $normalizedAddress = $flashAddress.Text
    }

    $regionStartAddress = $null
    $regionEndAddress = $null
    if ($RegionStart -and $RegionEnd) {
        $regionStartAddress = ConvertTo-FlashAddress $RegionStart "region start address"
        $regionEndAddress = ConvertTo-FlashAddress $RegionEnd "region end address"
        if ($regionStartAddress.Value -gt $regionEndAddress.Value) {
            throw "Region start address must not exceed region end address."
        }
        if (($flashAddress.Value -lt $regionStartAddress.Value) -or
            ($flashAddress.Value -gt $regionEndAddress.Value)) {
            throw "Flash address must be inside the bounded region."
        }
    }

    if ($firmwarePath) {
        $firmwareEndValue = $flashAddress.Value + $firmwareSize - 1L
        if ($firmwareEndValue -gt [UInt32]::MaxValue) {
            throw "Firmware exceeds the 32-bit address range."
        }
        if ($regionEndAddress -and $firmwareEndValue -gt $regionEndAddress.Value) {
            throw ("Firmware end address 0x{0:X8} exceeds region end address {1}." -f `
                $firmwareEndValue, $regionEndAddress.Text)
        }
    }

    $jlinkPath = Resolve-JLinkExecutable $JLinkExe
    $flashLoaderPath = Resolve-PY32F032FlashLoader $FlashLoader
    $deviceSupportDir = Join-Path ([IO.Path]::GetTempPath()) `
        ("jlink_py32f032_{0}" -f [Guid]::NewGuid().ToString("N"))
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

    $commandFile = Join-Path ([IO.Path]::GetTempPath()) ("jlink_flash_{0}.jlink" -f [Guid]::NewGuid().ToString("N"))
    $commands = @(
        "ExitOnError 1",
        "exec JLinkDevicesXMLPath = $deviceSupportDir",
        "device $Device"
    )
    if ($ResetOnly) {
        $commands += @("r", "g")
    }
    elseif ($EraseOnly) {
        $commands += @("r", "h", "le", "erase", "r", "g")
    }
    else {
        # J-Link V6.86 may incorrectly report Cortex-M0 as big endian after
        # the PY32 fallback reset. The target and CMSIS flash loader are LE.
        $commands += @("r", "h", "le")
        if ($EraseAll -or (-not $SkipErase -and -not $regionStartAddress)) {
            $commands += "erase"
        }
        $commands += "loadbin `"$firmwarePath`", $normalizedAddress"
        $commands += "verifybin `"$firmwarePath`", $normalizedAddress"
        if (-not $NoReset) { $commands += @("r", "g") }
    }
    $commands += "q"
    [IO.File]::WriteAllLines($commandFile, $commands, [Text.Encoding]::ASCII)

    Write-Host "[INFO] J-Link   : $jlinkPath"
    Write-Host "[INFO] Device   : $Device"
    Write-Host "[INFO] Interface: $Interface @ $Speed kHz"
    Write-Host "[INFO] Loader   : $flashLoaderPath"
    if ($ResetOnly) {
        Write-Host "[INFO] Operation: reset and run"
    }
    elseif ($EraseOnly) {
        Write-Host "[INFO] Operation: erase entire chip"
    }
    else {
        Write-Host "[INFO] Firmware : $firmwarePath"
        Write-Host "[INFO] Address  : $normalizedAddress"
    }
    if ($EraseOnly) {
        Write-Host "[INFO] Erase    : entire chip"
    }
    elseif ($regionStartAddress -and -not $ResetOnly) {
        Write-Host "[INFO] Region   : $($regionStartAddress.Text) - $($regionEndAddress.Text)"
        Write-Host "[INFO] Erase    : affected sectors selected by loadbin"
    }
    elseif ($SkipErase -and -not $ResetOnly) {
        Write-Host "[INFO] Erase    : skipped"
    }
    elseif (-not $ResetOnly) {
        Write-Host "[INFO] Erase    : entire chip"
    }

    $estimatedSeconds = if ($ResetOnly) {
        1.0
    }
    elseif ($EraseOnly) {
        3.0
    }
    else {
        Get-FlashEstimatedSeconds `
            -FileSize $firmwareSize `
            -SwdSpeedKHz $Speed `
            -EraseEnabled (-not $SkipErase)
    }
    Write-Host ("[INFO] Estimate : {0:N1} seconds ({1:N0} bytes)" -f `
        $estimatedSeconds, $firmwareSize)

    if ($DryRun) {
        Write-Host "[INFO] Dry run command file:" -ForegroundColor Yellow
        $commands | ForEach-Object { Write-Host "       $_" }
        exit 0
    }

    $jlinkArgs = @("-NoGui", "1", "-If", $Interface,
        "-Speed", $Speed, "-AutoConnect", "0", "-CommanderScript", $commandFile)
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

    $progressStatus = if ($ResetOnly) {
        "Resetting target"
    }
    elseif ($EraseOnly) {
        "Erasing flash"
    }
    else {
        "Programming and verifying"
    }
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    while ($flashJob.State -in @("NotStarted", "Running")) {
        Receive-Job -Job $flashJob | ForEach-Object { $jlinkOutput.Add($_.ToString()) }
        $percent = [Math]::Min(
            99,
            [Math]::Max(1, [Math]::Floor($stopwatch.Elapsed.TotalSeconds * 98 / $estimatedSeconds) + 1)
        )
        Set-FlashProgress $percent $progressStatus
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
    [IO.File]::WriteAllLines($logPath, $jlinkOutput, [Text.Encoding]::UTF8)

    $failurePattern = '(?i)(cannot connect|could not find.*emulator|failed to open|verification failed|verify failed|script processing failure|error while programming)'
    if ($exitCode -ne 0 -or (($jlinkOutput -join "`n") -match $failurePattern)) {
        Write-Host ""
        $jlinkOutput | ForEach-Object { Write-Host $_ }
        throw "J-Link programming failed (exit code: $exitCode). See log: $logPath"
    }

    Set-FlashProgress 0 "Completed" -Completed
    Write-Host ""
    $successText = if ($ResetOnly) {
        "Target reset completed successfully."
    }
    elseif ($EraseOnly) {
        "Chip erase completed successfully."
    }
    else {
        "Programming and verification completed successfully."
    }
    Write-Host "[OK] $successText" -ForegroundColor Green
    Write-Host "[INFO] Log      : $logPath"
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
    if ($deviceSupportDir -and (Test-Path -LiteralPath $deviceSupportDir -PathType Container)) {
        $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
        $supportPath = [IO.Path]::GetFullPath($deviceSupportDir)
        if ($supportPath.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) {
            Remove-Item -LiteralPath $supportPath -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}
