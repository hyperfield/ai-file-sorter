param(
    [string]$LibzipVersion = "1.11.4",
    [string]$PugixmlVersion = "1.15",
    [string]$PdfiumRelease = "latest"
)

$ErrorActionPreference = "Stop"

$rootDir = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$externalDir = Join-Path $rootDir "external"
$libzipDir = Join-Path $externalDir "libzip"
$pugixmlDir = Join-Path $externalDir "pugixml"
$pdfiumDir = Join-Path $externalDir "pdfium"
$licenseDir = Join-Path $externalDir "THIRD_PARTY_LICENSES"

function Ensure-Dir([string]$Path) {
    if (-not (Test-Path $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Require-Tool([string]$Name) {
    $tool = Get-Command $Name -ErrorAction SilentlyContinue
    if (-not $tool) {
        throw "$Name not found. Install it or ensure it is available in PATH."
    }
}

function Download-File([string]$Url, [string]$Destination) {
    Write-Output "Downloading $Url"
    Invoke-WebRequest -Uri $Url -OutFile $Destination
}

Require-Tool "tar"

Ensure-Dir $externalDir
Ensure-Dir $libzipDir
Ensure-Dir $pugixmlDir
Ensure-Dir $licenseDir
Ensure-Dir (Join-Path $pdfiumDir "linux-x64")
Ensure-Dir (Join-Path $pdfiumDir "windows-x64")
Ensure-Dir (Join-Path $pdfiumDir "macos-arm64")

$tempDir = Join-Path $env:TEMP "aifilesorter-docdeps"
Ensure-Dir $tempDir

$libzipArchive = Join-Path $tempDir "libzip-$LibzipVersion.tar.xz"
Download-File "https://libzip.org/download/libzip-$LibzipVersion.tar.xz" $libzipArchive
& tar -xf $libzipArchive -C $libzipDir --strip-components=1
if (Test-Path (Join-Path $libzipDir "LICENSE")) {
    Copy-Item (Join-Path $libzipDir "LICENSE") (Join-Path $licenseDir "libzip-LICENSE") -Force
}

$pugixmlArchive = Join-Path $tempDir "pugixml-$PugixmlVersion.tar.gz"
Download-File "https://github.com/zeux/pugixml/releases/download/v$PugixmlVersion/pugixml-$PugixmlVersion.tar.gz" $pugixmlArchive
& tar -xf $pugixmlArchive -C $pugixmlDir --strip-components=1
if (Test-Path (Join-Path $pugixmlDir "LICENSE.md")) {
    Copy-Item (Join-Path $pugixmlDir "LICENSE.md") (Join-Path $licenseDir "pugixml-LICENSE.md") -Force
} elseif (Test-Path (Join-Path $pugixmlDir "LICENSE")) {
    Copy-Item (Join-Path $pugixmlDir "LICENSE") (Join-Path $licenseDir "pugixml-LICENSE") -Force
}

$pdfiumLinuxArchive = Join-Path $tempDir "pdfium-linux-x64.tgz"
Download-File "https://github.com/bblanchon/pdfium-binaries/releases/$PdfiumRelease/download/pdfium-linux-x64.tgz" $pdfiumLinuxArchive
& tar -xf $pdfiumLinuxArchive -C (Join-Path $pdfiumDir "linux-x64")

$pdfiumWinArchive = Join-Path $tempDir "pdfium-win-x64.tgz"
Download-File "https://github.com/bblanchon/pdfium-binaries/releases/$PdfiumRelease/download/pdfium-win-x64.tgz" $pdfiumWinArchive
& tar -xf $pdfiumWinArchive -C (Join-Path $pdfiumDir "windows-x64")

$pdfiumMacArchive = Join-Path $tempDir "pdfium-mac-arm64.tgz"
Download-File "https://github.com/bblanchon/pdfium-binaries/releases/$PdfiumRelease/download/pdfium-mac-arm64.tgz" $pdfiumMacArchive
& tar -xf $pdfiumMacArchive -C (Join-Path $pdfiumDir "macos-arm64")

if (Test-Path (Join-Path $pdfiumDir "linux-x64\LICENSE")) {
    Copy-Item (Join-Path $pdfiumDir "linux-x64\LICENSE") (Join-Path $licenseDir "pdfium-LICENSE") -Force
} elseif (Test-Path (Join-Path $pdfiumDir "linux-x64\LICENSE.txt")) {
    Copy-Item (Join-Path $pdfiumDir "linux-x64\LICENSE.txt") (Join-Path $licenseDir "pdfium-LICENSE.txt") -Force
}

$pdfiumReadme = @"
# PDFium prebuilts

This folder is populated by `app/scripts/vendor_doc_deps.sh` or `app/scripts/vendor_doc_deps.ps1`.
Expected layout:

- linux-x64/
- windows-x64/
- macos-arm64/

Each folder should contain `include/` and the platform PDFium library under `lib/`:

- Linux: `lib/libpdfium.so`
- Windows: `bin/pdfium.dll` + `lib/pdfium.dll.lib`
- macOS: `lib/libpdfium.dylib`
"@
Set-Content -Path (Join-Path $pdfiumDir "README.md") -Value $pdfiumReadme

Write-Output "Done. You can now commit external/libzip, external/pugixml, and external/pdfium."
