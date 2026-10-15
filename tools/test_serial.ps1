$sp = New-Object System.IO.Ports.SerialPort('COM10', 19200, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
$sp.ReadTimeout = 1000
$sp.Open()

# Read any pending output from initial frame
Start-Sleep -Milliseconds 200
$data0 = $sp.ReadExisting()
if ($data0) { Write-Host "Pending output: $data0" }

Write-Host "Sending Super Shotgun FIRE (SPACE)..."
$sp.Write(" ")
Start-Sleep -Milliseconds 400
$data1 = $sp.ReadExisting()
Write-Host "Response: $data1"

Write-Host "Sending Forward Movement ('w' x 10)..."
for ($i = 0; $i -lt 10; $i++) {
    $sp.Write("w")
    Start-Sleep -Milliseconds 100
}
Start-Sleep -Milliseconds 400
$data2 = $sp.ReadExisting()
Write-Host "Response: $data2"

$sp.Close()
