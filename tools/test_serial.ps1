$sp = New-Object System.IO.Ports.SerialPort('COM10', 19200, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
$sp.ReadTimeout = 1000
$sp.Open()

Write-Host "Listening to live continuous DOOM 2 telemetry for 4 seconds..."
$sw = [System.Diagnostics.Stopwatch]::StartNew()
while ($sw.ElapsedMilliseconds -lt 4000) {
    Start-Sleep -Milliseconds 200
    $data = $sp.ReadExisting()
    if ($data) {
        Write-Host $data -NoNewline
    }
}

$sp.Close()
