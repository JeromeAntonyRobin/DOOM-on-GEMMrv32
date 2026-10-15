param(
    [string]$Port = "COM10",
    [int]$Baud = 19200,
    [string]$BinFile = "bin\demo_vga_doom.bin"
)

if (-not (Test-Path $BinFile)) {
    Write-Error "File not found: $BinFile"
    exit 1
}

$bytes = [System.IO.File]::ReadAllBytes((Resolve-Path $BinFile))
$sp = New-Object System.IO.Ports.SerialPort($Port, $Baud, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
$sp.ReadTimeout = 1000
$sp.WriteTimeout = 2000

try {
    $sp.Open()
    Write-Host "Opened ${Port} at $Baud baud."
} catch {
    Write-Error "Failed to open ${Port}: $_"
    exit 1
}

# Clear any buffers
$sp.DiscardInBuffer()
$sp.DiscardOutBuffer()

Write-Host "Synchronizing with NEORV32 bootloader prompt..."
$found_prompt = $false
for ($attempt = 0; $attempt -lt 10; $attempt++) {
    $sp.Write("`r`n")
    Start-Sleep -Milliseconds 250
    $resp = $sp.ReadExisting()
    if ($resp -match "CMD:>") {
        $found_prompt = $true
        Write-Host "Found CMD:> prompt."
        break
    }
}

if (-not $found_prompt) {
    Write-Host "Prompt not detected immediately, sending reset character 'r'..."
    $sp.Write("r`r`n")
    Start-Sleep -Milliseconds 400
    $resp = $sp.ReadExisting()
    Write-Host $resp
}

Write-Host "Sending 'u' command to enter upload mode..."
$sp.DiscardInBuffer()
$sp.Write("u")

$awaiting = $false
$sw = [System.Diagnostics.Stopwatch]::StartNew()
while ($sw.ElapsedMilliseconds -lt 4000) {
    Start-Sleep -Milliseconds 150
    $resp = $sp.ReadExisting()
    if ($resp) {
        Write-Host $resp -NoNewline
    }
    if ($resp -match "Awaiting") {
        $awaiting = $true
        break
    }
}

if (-not $awaiting) {
    Write-Error "`nBootloader did not confirm upload mode."
    $sp.Close()
    exit 1
}

Write-Host "`nUploading $($bytes.Length) bytes of authentic DOOM 2 executable (est. 16s at 19200 baud)..."
$chunk_size = 512
for ($i = 0; $i -lt $bytes.Length; $i += $chunk_size) {
    $count = [Math]::Min($chunk_size, $bytes.Length - $i)
    $sp.Write($bytes, $i, $count)
    $pct = [Math]::Round(($i + $count) * 100 / $bytes.Length)
    Write-Host "`rProgress: $pct% ($($i + $count)/$($bytes.Length) bytes)" -NoNewline
    Start-Sleep -Milliseconds 270
}

Write-Host "`nWaiting for transmission buffer to flush..."
while ($sp.BytesToWrite -gt 0) {
    Start-Sleep -Milliseconds 100
}

Write-Host "Awaiting bootloader acknowledgment (OK)..."
$sw = [System.Diagnostics.Stopwatch]::StartNew()
$ack = $false
while ($sw.ElapsedMilliseconds -lt 5000) {
    Start-Sleep -Milliseconds 200
    $resp = $sp.ReadExisting()
    if ($resp) {
        Write-Host $resp -NoNewline
    }
    if ($resp -match "OK" -or $resp -match "CMD:>") {
        $ack = $true
        break
    }
}

Write-Host "`nSending 'e' command to start authentic DOOM 2..."
$sp.Write("e")

Write-Host "=== DOOM 2 OUTPUT ==="
$sw = [System.Diagnostics.Stopwatch]::StartNew()
while ($sw.ElapsedMilliseconds -lt 4000) {
    Start-Sleep -Milliseconds 200
    $extra = $sp.ReadExisting()
    if ($extra) {
        Write-Host $extra -NoNewline
    }
}

$sp.Close()
Write-Host "`nUpload session complete."
