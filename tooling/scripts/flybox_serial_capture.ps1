# Flybox serial capture — opens COM25 first, triggers MCU reset via OpenOCD,
# then drains the buffer at 25ms cadence with a 64KB read buffer so the
# initial 1KB-burst of boot output is not lost to USB-UART purge-on-open.
param(
    [int]    $WaitSec = 30,
    [string] $LogFile = 'D:\temp\flybox_serial.log',
    [string] $Port    = 'COM25',
    [int]    $Baud    = 115200
)

# NOTE: Do NOT set ErrorActionPreference='Stop' — openocd writes its banner
# to stderr, which Windows PowerShell treats as a NativeCommandError and
# aborts the script. Keep default ('Continue') so stderr is just printed.

# Prepare log
'' | Set-Content $LogFile -Encoding ascii

$sp = New-Object System.IO.Ports.SerialPort $Port, $Baud, 'None', 8, 'One'
$sp.ReadBufferSize = 65536
$sp.ReadTimeout    = 100
# DTR/RTS asserted — some USB-UART bridges (CH340 family) require these
# high or they latch into a "stop receiving" state after the first burst.
$sp.DtrEnable      = $true
$sp.RtsEnable      = $true
$sp.Handshake      = 'None'
$sp.Open()

# Trigger MCU reset with the port already streaming.
# Redirect stderr (banner) + stdout to a temp file then ignore.
$ocdLog = Join-Path $env:TEMP 'flybox_ocd_reset.log'
& openocd -f interface/cmsis-dap.cfg `
    -c 'adapter speed 1000' `
    -f target/stm32f4x.cfg `
    -c 'cortex_m reset_config sysresetreq' `
    -c 'init' -c 'reset run' -c 'shutdown' `
    > $ocdLog 2>&1

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
