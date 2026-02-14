param(
    [Parameter(Mandatory = $true)][string]$BasePng,
    [Parameter(Mandatory = $true)][string]$Ico
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

$basePath = Resolve-Path $BasePng
if (-not $basePath) {
    throw "Base PNG '$BasePng' does not exist."
}

$icoFullPath = [System.IO.Path]::GetFullPath($Ico)
$icoDir = [System.IO.Path]::GetDirectoryName($icoFullPath)
if ($icoDir -and -not (Test-Path $icoDir)) {
    New-Item -ItemType Directory -Force -Path $icoDir | Out-Null
}

$bitmap = [System.Drawing.Bitmap]::FromFile($basePath)
try {
    $width = $bitmap.Width
    $height = $bitmap.Height
    $stream = New-Object System.IO.MemoryStream
    $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
    $bytes = $stream.ToArray()
    $stream.Dispose()
}
finally {
    $bitmap.Dispose()
}

$fs = [System.IO.File]::Open($icoFullPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
$bw = New-Object System.IO.BinaryWriter($fs)

try {
    $bw.Write([UInt16]0)
    $bw.Write([UInt16]1)
    $bw.Write([UInt16]1)

    $widthByte  = if ($width  -ge 256) { 0 } else { [byte]$width }
    $heightByte = if ($height -ge 256) { 0 } else { [byte]$height }
    $bw.Write([byte]$widthByte)
    $bw.Write([byte]$heightByte)
    $bw.Write([byte]0)
    $bw.Write([byte]0)
    $bw.Write([UInt16]1)
    $bw.Write([UInt16]32)
    $bw.Write([UInt32]$bytes.Length)
    $bw.Write([UInt32](6 + 16))
    $bw.Write($bytes)
}
finally {
    $bw.Dispose()
    $fs.Dispose()
}

