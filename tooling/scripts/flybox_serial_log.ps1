# Flybox serial logger — runs in background, writes to log file.
# Usage:
#   powershell -File flybox_serial_log.ps1                   # default COM25, 115200
#   powershell -File flybox_serial_log.ps1 -Port COM7        # override port
#   powershell -File flybox_serial_log.ps1 -Seconds 120      # run for 2 minutes
#
# Read output:  Read tool on $env:TEMP\flybox_serial.log
# Stop:         the process exits after -Seconds (default 300 = 5 min)
param(
    [string]$Port = "COM25",
    [int]$BaudRate = 115200,
    [int]$Seconds = 300,
    [string]$LogFile = "$env:TEMP\flybox_serial.log"
)

"" | Set-Content $LogFile -Encoding utf8
"[SERIAL] Logging $Port @ $BaudRate to $LogFile for ${Seconds}s" | Tee-Object $LogFile -Append

try {
    $sp = New-Object System.IO.Ports.SerialPort $Port, $BaudRate, 'None', 8, 'One'
    $sp.ReadTimeout = 1000
    $sp.Open()

    $end = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $end) {
        try {
            $data = $sp.ReadExisting()
            if ($data.Length -gt 0) {
                $data | Out-File $LogFile -Append -Encoding utf8 -NoNewline
            }
        } catch [System.TimeoutException] { }
        Start-Sleep -Milliseconds 100
    }

    $sp.Close()
    "[SERIAL] Done." | Out-File $LogFile -Append -Encoding utf8
} catch {
    "[SERIAL] ERROR: $_" | Out-File $LogFile -Append -Encoding utf8
    if ($sp -and $sp.IsOpen) { $sp.Close() }
    exit 1
}
