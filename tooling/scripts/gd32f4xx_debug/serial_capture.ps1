# Generic serial capture for STM32-family / GD32 SWD targets.
# Opens the serial port BEFORE triggering reset via OpenOCD so the
# initial 1KB-burst of boot output is not lost to USB-UART purge-on-open.
#
# CH340 USB-UART chips need DTR/RTS asserted high or they latch into a
# "stop receiving" state after the first burst. This is wired in by default.
#
# Usage:
#   pwsh -File serial_capture.ps1 -Port COM25
#   pwsh -File serial_capture.ps1 -Port COM25 -WaitSec 60 -LogFile out.log
#   pwsh -File serial_capture.ps1 -Port COM25 -OcdTarget target/stm32f1x.cfg
#
# Note: target/stm32f4x.cfg works for GD32F4xx (SWD-compatible).

param(
    [Parameter(Mandatory=$true)]
    [string] $Port,

    [int]    $Baud         = 115200,
    [int]    $WaitSec      = 30,
    [string] $LogFile      = 'serial.log',
    [string] $OcdInterface = 'interface/cmsis-dap.cfg',
    [string] $OcdTarget    = 'target/stm32f4x.cfg',
    [int]    $OcdSpeed     = 1000,
    [switch] $NoReset
)

# NOTE: Do NOT set ErrorActionPreference='Stop' — openocd writes its banner
# to stderr, which Windows PowerShell treats as a NativeCommandError and
# aborts the script. Keep default ('Continue').

# Prepare log
'' | Set-Content $LogFile -Encoding ascii

$sp = New-Object System.IO.Ports.SerialPort $Port, $Baud, 'None', 8, 'One'
$sp.ReadBufferSize = 65536
$sp.ReadTimeout    = 100
# DTR/RTS asserted — CH340 family requires these high or RX latches off.
$sp.DtrEnable      = $true
$sp.RtsEnable      = $true
$sp.Handshake      = 'None'
$sp.Open()

if (-not $NoReset) {
    # Trigger MCU reset with the port already streaming.
    $ocdLog = Join-Path $env:TEMP 'serial_capture_ocd.log'
    & openocd -f $OcdInterface `
        -c "adapter speed $OcdSpeed" `
        -f $OcdTarget `
        -c 'cortex_m reset_config sysresetreq' `
        -c 'init' -c 'reset run' -c 'shutdown' `
        > $ocdLog 2>&1
}

# Drain loop. Larger buffer + tight poll to avoid losing bursts.
$end = (Get-Date).AddSeconds($WaitSec)
$writer = [IO.StreamWriter]::new($LogFile, $true, [Text.Encoding]::ASCII)
try {
    while ((Get-Date) -lt $end) {
        try {
            $d = $sp.ReadExisting()
            if ($d.Length -gt 0) {
                $writer.Write($d)
                $writer.Flush()
            }
        } catch {}
        Start-Sleep -Milliseconds 25
    }
} finally {
    $writer.Close()
    $sp.Close()
}
