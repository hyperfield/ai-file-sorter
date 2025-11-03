$ErrorActionPreference = "Stop"

# --- Parse optional arguments ---
$useCuda = "OFF"
$vcpkgRootArg = $null
foreach ($arg in $args) {
    if ($arg -match "^cuda=(on|off)$") {
        $useCuda = $Matches[1].ToUpper()
    } elseif ($arg -match "^vcpkgroot=(.+)$") {
        $vcpkgRootArg = $Matches[1]
    }
}

Write-Host "`nCUDA Support: $useCuda`n"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$llamaDir = Join-Path $scriptDir "..\include\external\llama.cpp"

if (-not (Test-Path $llamaDir)) {
    Write-Host "Missing llama.cpp submodule. Please run:"
    Write-Host "  git submodule update --init --recursive"
    exit 1
}

$precompiledRootDir = Join-Path $scriptDir "..\lib\precompiled"
$headersDir = Join-Path $scriptDir "..\include\llama"
$ggmlRuntimeRoot = Join-Path $scriptDir "..\lib\ggml"

# --- Locate cmake executable ---
function Resolve-CMake {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }
    $vsCMake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (Test-Path $vsCMake) {
        return $vsCMake
    }
    throw "cmake executable not found in PATH. Run this script from a VS Developer PowerShell or install CMake and ensure it is on PATH."
}

$cmakeExe = Resolve-CMake

# --- Locate OpenBLAS (required on Windows) ---
function Resolve-VcpkgRoot {
    param([string]$Explicit)

    if ($Explicit) { return $Explicit }

    $defaultRoot = "C:\dev\vcpkg"
    if (Test-Path $defaultRoot) {
        return $defaultRoot
    }

    if ($env:VCPKG_ROOT) {
        if ($env:VCPKG_ROOT -like "*Program Files*Microsoft Visual Studio*") {
            Write-Warning "Detected Visual Studio's bundled vcpkg at '$($env:VCPKG_ROOT)', which is typically read-only. Please clone vcpkg to a writable location (e.g. $defaultRoot) and pass vcpkgroot=<path>."
            return $null
        }
        return $env:VCPKG_ROOT
    }

    return $null
}

$vcpkgRoot = Resolve-VcpkgRoot -Explicit $vcpkgRootArg
if (-not $vcpkgRoot -or -not (Test-Path $vcpkgRoot)) {
    throw "Could not resolve a writable vcpkg root. Pass vcpkgroot=<path> (e.g. C:\dev\vcpkg) or set VCPKG_ROOT accordingly."
}
$env:VCPKG_ROOT = $vcpkgRoot

$triplet = "x64-windows"

function Invoke-Vcpkg {
    param(
        [string]$Subcommand,
        [string[]]$PackageArgs = @()
    )
    $vcpkgExe = Join-Path $vcpkgRoot "vcpkg.exe"
    if (-not (Test-Path $vcpkgExe)) {
        throw "Cannot find vcpkg.exe under $vcpkgRoot. Please ensure vcpkg is installed there."
    }
    Push-Location $vcpkgRoot
    Write-Host "Invoking vcpkg with arguments: $Subcommand $($PackageArgs -join ' ') (count=$($PackageArgs.Count))"
    if ($PackageArgs.Count -eq 0) {
        & $vcpkgExe "--vcpkg-root" $vcpkgRoot $Subcommand
    } else {
        & $vcpkgExe "--vcpkg-root" $vcpkgRoot $Subcommand @PackageArgs
    }
    $exit = $LASTEXITCODE
    Pop-Location
    if ($exit -ne 0) {
        throw "vcpkg $Subcommand failed with exit code $exit"
    }
}

function Confirm-VcpkgPackage {
    param(
        [string]$HeaderCheckPath,
        [string]$LibraryCheckPath,
        [string]$PackageName
    )
    if (-not (Test-Path $HeaderCheckPath) -or -not (Test-Path $LibraryCheckPath)) {
        Write-Host "$PackageName not found. Installing via vcpkg ..."
        $pkgSpec = "${PackageName}:$triplet"
        Write-Host "Running: vcpkg install $pkgSpec"
        Invoke-Vcpkg -Subcommand "install" -PackageArgs @($pkgSpec)
    }
}

# NOTE: Temporarily disable OpenBLAS handling.
# $openBlasInclude = Join-Path $vcpkgRoot "installed\$triplet\include"
# $openBlasIncludeSub = Join-Path $openBlasInclude "openblas"
# $openBlasLib = Join-Path $vcpkgRoot "installed\$triplet\lib\openblas.lib"
# $openBlasDll = Join-Path $vcpkgRoot "installed\$triplet\bin\openblas.dll"
# Confirm-VcpkgPackage -HeaderCheckPath (Join-Path $openBlasIncludeSub "cblas.h") -LibraryCheckPath $openBlasLib -PackageName "openblas"

$curlInclude = Join-Path $vcpkgRoot "installed\$triplet\include"
$curlLib = Join-Path $vcpkgRoot "installed\$triplet\lib\libcurl.lib"
$curlDll = Join-Path $vcpkgRoot "installed\$triplet\bin\libcurl.dll"
Confirm-VcpkgPackage -HeaderCheckPath (Join-Path $curlInclude "curl\curl.h") -LibraryCheckPath $curlLib -PackageName "curl"

# Write-Host "Using OpenBLAS include: $openBlasInclude"
# Write-Host "Using OpenBLAS lib: $openBlasLib"

# --- Build from llama.cpp ---
Push-Location $llamaDir

if (Test-Path "build") {
    Remove-Item -Recurse -Force "build"
}
New-Item -ItemType Directory -Path "build" | Out-Null

$cmakeArgs = @(
    "-DCMAKE_C_COMPILER=`"C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe`"",
    "-DCMAKE_CXX_COMPILER=`"C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe`"",
    "-DCURL_LIBRARY=`"$curlLib`"",
    "-DCURL_INCLUDE_DIR=`"$curlInclude`"",
    "-DBUILD_SHARED_LIBS=ON",
    # Temporarily keep BLAS disabled while OpenBLAS is removed.
    "-DGGML_BLAS=OFF",
    "-DGGML_OPENCL=OFF",
    "-DGGML_VULKAN=OFF",
    "-DGGML_SYCL=OFF",
    "-DGGML_HIP=OFF",
    "-DGGML_KLEIDIAI=OFF",
    "-DGGML_NATIVE=OFF",
    "-DCMAKE_C_FLAGS=/arch:AVX2",
    "-DCMAKE_CXX_FLAGS=/arch:AVX2"
)

if ($useCuda -eq "ON") {
    $cudaRoot = "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.9"
    $includeDir = "$cudaRoot/include"
    $libDir = "$cudaRoot/lib/x64/cudart.lib"

    $cmakeArgs += @(
        "-DGGML_CUDA=ON",
        "-DCUDA_TOOLKIT_ROOT_DIR=`"$cudaRoot`"",
        "-DCUDA_INCLUDE_DIRS=`"$includeDir`"",
        "-DCUDA_CUDART=`"$libDir`""
    )
} else {
    $cmakeArgs += "-DGGML_CUDA=OFF"
}

& $cmakeExe -S . -B build @cmakeArgs
& $cmakeExe --build build --config Release -- /m

Pop-Location

# --- Clean and repopulate precompiled outputs ---
$variant = if ($useCuda -eq "ON") { "cuda" } else { "cpu" }
$runtimeSubdir = if ($useCuda -eq "ON") { "wcuda" } else { "wocuda" }
$variantRoot = Join-Path $precompiledRootDir $variant
$variantBin = Join-Path $variantRoot "bin"
$variantLib = Join-Path $variantRoot "lib"
$runtimeDir = Join-Path $ggmlRuntimeRoot $runtimeSubdir

foreach ($dir in @($variantBin, $variantLib, $runtimeDir)) {
    if (Test-Path $dir) {
        Remove-Item -Recurse -Force $dir
    }
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
}

$releaseBin = Join-Path $llamaDir "build\bin\Release"
$dllList = @("llama.dll", "ggml.dll", "ggml-base.dll", "ggml-cpu.dll")
$optionalDlls = @("ggml-blas.dll", "ggml-openblas.dll")
foreach ($maybeDll in $optionalDlls) {
    $candidate = Join-Path $releaseBin $maybeDll
    if (Test-Path $candidate) {
        $dllList += $maybeDll
    }
}
if ($useCuda -eq "ON") {
    $dllList += "ggml-cuda.dll"
}

foreach ($dll in $dllList) {
    $src = Join-Path $releaseBin $dll
    if (-not (Test-Path $src)) {
        throw "Expected DLL '$dll' not found at $src"
    }
    Copy-Item $src -Destination $variantBin -Force
    Copy-Item $src -Destination $runtimeDir -Force
}

# if (Test-Path $openBlasDll) {
#     $libOpenBlasName = "libopenblas.dll"
#     Copy-Item $openBlasDll -Destination (Join-Path $variantBin $libOpenBlasName) -Force
#     Copy-Item $openBlasDll -Destination (Join-Path $runtimeDir $libOpenBlasName) -Force
#     foreach ($legacy in @((Join-Path $variantBin "openblas.dll"), (Join-Path $runtimeDir "openblas.dll"))) {
#         if (Test-Path $legacy) {
#             Remove-Item $legacy -Force
#         }
#     }
# }
if (Test-Path $curlDll) {
    Copy-Item $curlDll -Destination $variantBin -Force
    Copy-Item $curlDll -Destination $runtimeDir -Force
}

$importLibNames = @("llama.lib", "ggml.lib", "ggml-base.lib", "ggml-cpu.lib")
$optionalLibs = @("ggml-blas.lib", "ggml-openblas.lib")
if ($useCuda -eq "ON") {
    $importLibNames += "ggml-cuda.lib"
}

foreach ($libName in $importLibNames) {
    $libSource = Get-ChildItem (Join-Path $llamaDir "build") -Filter $libName -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $libSource) {
        throw "Could not locate $libName within the llama.cpp build directory."
    }
    Copy-Item $libSource.FullName -Destination (Join-Path $variantLib $libName) -Force
}
foreach ($libName in $optionalLibs) {
    $libSource = Get-ChildItem (Join-Path $llamaDir "build") -Filter $libName -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($libSource) {
        Copy-Item $libSource.FullName -Destination (Join-Path $variantLib $libName) -Force
    }
}

# --- Copy headers ---
New-Item -ItemType Directory -Force -Path $headersDir | Out-Null
Copy-Item "$llamaDir\include\llama.h" -Destination $headersDir
Copy-Item "$llamaDir\ggml\src\*.h" -Destination $headersDir -ErrorAction SilentlyContinue
Copy-Item "$llamaDir\ggml\include\*.h" -Destination $headersDir -ErrorAction SilentlyContinue
