$sp = New-Object System.IO.Ports.SerialPort('COM10', 19200, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
$sp.ReadTimeout = 1000
$sp.Open()

Write-Host "Firing Super Shotgun again..."
$sp.Write(" ")
Start-Sleep -Milliseconds 400
Write-Host $sp.ReadExisting()

Write-Host "Turning and moving..."
$sp.Write("d")
Start-Sleep -Milliseconds 100
$sp.Write("w")
Start-Sleep -Milliseconds 100
$sp.Write("w")
Start-Sleep -Milliseconds 100
$sp.Write("w")
Start-Sleep -Milliseconds 100
$sp.Write("w")
Start-Sleep -Milliseconds 100
$sp.Write("w")
Start-Sleep -Milliseconds 100
$sp.Write("w")
Start-Sleep -Milliseconds 100
$sp.Write("w")
Start-Sleep -Milliseconds 100
$sp.Write("w")
Start-Sleep -Milliseconds 100
$sp.Write("w")
Start-Sleep -Milliseconds 100
$sp.Write("w")
Start-Sleep -Milliseconds 400
Write-Host $sp.ReadExisting()

$sp.Close()
