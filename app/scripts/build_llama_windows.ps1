$ErrorActionPreference = "Stop"

# --- Parse optional arguments ---
$useCuda = "OFF"
$useVulkan = "OFF"
$vcpkgRootArg = $null
$openBlasRootArg = $null
foreach ($arg in $args) {
    if ($arg -match "^cuda=(on|off)$") {
        $useCuda = $Matches[1].ToUpper()
    } elseif ($arg -match "^vulkan=(on|off)$") {
        $useVulkan = $Matches[1].ToUpper()
    } elseif ($arg -match "^vcpkgroot=(.+)$") {
        $vcpkgRootArg = $Matches[1]
    } elseif ($arg -match "^openblasroot=(.+)$") {
        $openBlasRootArg = $Matches[1]
    }
}

if ($useCuda -eq "ON" -and $useVulkan -eq "ON") {
    throw "Cannot enable both CUDA and Vulkan simultaneously. Choose only one backend."
}

Write-Output "`nCUDA Support: $useCuda`n"
Write-Output "Vulkan Support: $useVulkan`n"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$llamaDir = Join-Path $scriptDir "..\include\external\llama.cpp"

if (-not (Test-Path $llamaDir)) {
    Write-Output "Missing llama.cpp submodule. Please run:"
    Write-Output "  git submodule update --init --recursive"
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

$cpuOnlyBuild = ($useCuda -eq "OFF" -and $useVulkan -eq "OFF")

function Resolve-OpenBlasRoot {
    param([string]$Explicit)

    $candidates = @()
    if ($Explicit) { $candidates += $Explicit }
    if ($env:OPENBLAS_ROOT) { $candidates += $env:OPENBLAS_ROOT }
    $candidates += "C:\msys64\mingw64"

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
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
    Write-Output "Invoking vcpkg with arguments: $Subcommand $($PackageArgs -join ' ') (count=$($PackageArgs.Count))"
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
        [string]$PackageName,
        [string[]]$AdditionalPaths = @()
    )

    $pathsToCheck = @()
    if ($HeaderCheckPath) { $pathsToCheck += $HeaderCheckPath }
    if ($LibraryCheckPath) { $pathsToCheck += $LibraryCheckPath }
    foreach ($extraPath in $AdditionalPaths) {
        if ($extraPath) { $pathsToCheck += $extraPath }
    }

    if ($pathsToCheck.Count -eq 0) {
        throw "Confirm-VcpkgPackage was called for $PackageName with no paths to validate."
    }

    $needsInstall = $false
    foreach ($candidate in $pathsToCheck) {
        if (-not (Test-Path $candidate)) {
            $needsInstall = $true
            break
        }
    }

    if ($needsInstall) {
        Write-Output "$PackageName not found. Installing via vcpkg ..."
        $pkgSpec = "${PackageName}:$triplet"
        Write-Output "Running: vcpkg install $pkgSpec"
        Invoke-Vcpkg -Subcommand "install" -PackageArgs @($pkgSpec)
    }

    foreach ($candidate in $pathsToCheck) {
        if (-not (Test-Path $candidate)) {
            throw "Expected $candidate from package $PackageName but the path is still missing."
        }
    }
}

$curlInclude = Join-Path $vcpkgRoot "installed\$triplet\include"
$curlLib = Join-Path $vcpkgRoot "installed\$triplet\lib\libcurl.lib"
$curlDll = Join-Path $vcpkgRoot "installed\$triplet\bin\libcurl.dll"
Confirm-VcpkgPackage -HeaderCheckPath (Join-Path $curlInclude "curl\curl.h") -LibraryCheckPath $curlLib -PackageName "curl"

$openBlasInclude = $null
$openBlasLib = $null
$openBlasDll = $null
if ($cpuOnlyBuild) {
    $openBlasRoot = Resolve-OpenBlasRoot -Explicit $openBlasRootArg
    if (-not $openBlasRoot) {
        throw "CPU-only builds require OpenBLAS from MSYS2/MinGW64. Pass openblasroot=<path> or set OPENBLAS_ROOT."
    }

    $openBlasIncludeRoot = Join-Path $openBlasRoot "include"
    $openBlasInclude = Join-Path $openBlasIncludeRoot "openblas"
    $openBlasHeader = Join-Path $openBlasInclude "cblas.h"
    if (-not (Test-Path $openBlasHeader)) {
        throw "Missing cblas.h under $openBlasInclude. Install OpenBLAS via MSYS2 (pacman -S mingw-w64-x86_64-openblas) or point openblasroot to a valid tree."
    }

    $openBlasLibCandidates = @(
        (Join-Path $openBlasRoot "lib\openblas.lib")
        (Join-Path $openBlasRoot "lib\libopenblas.lib")
        (Join-Path $openBlasRoot "lib\libopenblas.dll.a")
        (Join-Path $openBlasRoot "lib\libopenblas.a")
    )
    foreach ($candidate in $openBlasLibCandidates) {
        if (Test-Path $candidate) {
            $openBlasLib = $candidate
            break
        }
    }
    if (-not $openBlasLib) {
        throw "Could not find an OpenBLAS import library under $openBlasRoot\lib."
    }

    $openBlasDllCandidates = @(
        (Join-Path $openBlasRoot "bin\libopenblas.dll")
        (Join-Path $openBlasRoot "bin\openblas.dll")
    )
    foreach ($candidate in $openBlasDllCandidates) {
        if (Test-Path $candidate) {
            $openBlasDll = $candidate
            break
        }
    }
    if (-not $openBlasDll) {
        throw "Could not find the OpenBLAS runtime DLL (e.g. libopenblas.dll) under $openBlasRoot\bin."
    }

    Write-Host "Using OpenBLAS from $openBlasRoot"
}

$vulkanIncludeDir = $null
$vulkanLibPath = $null
$vulkanDllPath = $null
$vulkanGlslcPath = $null
$vulkanSdkRoot = $null
if ($useVulkan -eq "ON") {
    $vulkanIncludeDir = Join-Path $vcpkgRoot "installed\$triplet\include"
    $vulkanHeaderPath = Join-Path $vulkanIncludeDir "vulkan\vulkan.h"
    $vulkanLibPath = Join-Path $vcpkgRoot "installed\$triplet\lib\vulkan-1.lib"
    $vulkanDllPath = Join-Path $vcpkgRoot "installed\$triplet\bin\vulkan-1.dll"
    $shadercToolsDir = Join-Path $vcpkgRoot "installed\$triplet\tools\shaderc"
    $vulkanGlslcPath = Join-Path $shadercToolsDir "glslc.exe"

    Confirm-VcpkgPackage -HeaderCheckPath $vulkanHeaderPath -LibraryCheckPath $null -PackageName "vulkan-headers"
    Confirm-VcpkgPackage -HeaderCheckPath $null -LibraryCheckPath $vulkanLibPath -PackageName "vulkan-loader" -AdditionalPaths @($vulkanDllPath)
    Confirm-VcpkgPackage -HeaderCheckPath $null -LibraryCheckPath $null -PackageName "shaderc" -AdditionalPaths @($vulkanGlslcPath)

    $vulkanSdkRoot = Join-Path $vcpkgRoot "installed\$triplet"
    $env:VULKAN_SDK = $vulkanSdkRoot
}

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
    "-DGGML_OPENCL=OFF",
    "-DGGML_VULKAN=$useVulkan",
    "-DGGML_SYCL=OFF",
    "-DGGML_HIP=OFF",
    "-DGGML_KLEIDIAI=OFF",
    "-DGGML_NATIVE=OFF",
    "-DCMAKE_C_FLAGS=/arch:AVX2",
    "-DCMAKE_CXX_FLAGS=/arch:AVX2"
)

if ($cpuOnlyBuild) {
    if (-not $openBlasInclude -or -not $openBlasLib) {
        throw "OpenBLAS paths not initialized for the CPU-only build."
    }
    $cmakeArgs += @(
        "-DGGML_BLAS=ON",
        "-DGGML_BLAS_VENDOR=OpenBLAS",
        "-DBLA_VENDOR=OpenBLAS",
        "-DBLAS_INCLUDE_DIRS=`"$openBlasInclude`"",
        "-DBLAS_LIBRARIES=`"$openBlasLib`""
    )
} else {
    $cmakeArgs += "-DGGML_BLAS=OFF"
}

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

if ($useVulkan -eq "ON") {
    if (-not $vulkanIncludeDir -or -not $vulkanLibPath -or -not $vulkanGlslcPath) {
        throw "Vulkan paths were not initialized even though Vulkan support is enabled."
    }
    $cmakeArgs += @(
        "-DVulkan_INCLUDE_DIR=`"$vulkanIncludeDir`"",
        "-DVulkan_LIBRARY=`"$vulkanLibPath`"",
        "-DVulkan_GLSLC_EXECUTABLE=`"$vulkanGlslcPath`""
    )
    if ($vulkanSdkRoot) {
        $cmakeArgs += "-DVULKAN_SDK=`"$vulkanSdkRoot`""
    }
}

& $cmakeExe -S . -B build @cmakeArgs
& $cmakeExe --build build --config Release -- /m

Pop-Location

# --- Clean and repopulate precompiled outputs ---
$variant = "cpu"
$runtimeSubdir = "wocuda"
if ($useCuda -eq "ON") {
    $variant = "cuda"
    $runtimeSubdir = "wcuda"
} elseif ($useVulkan -eq "ON") {
    $variant = "vulkan"
    $runtimeSubdir = "wvulkan"
}
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
$dllList = @()
if (Test-Path $releaseBin) {
    $dllList = Get-ChildItem -Path $releaseBin -Filter "*.dll" -File | Select-Object -ExpandProperty Name
}
if (-not $dllList -or $dllList.Count -eq 0) {
    throw "No DLLs were produced in $releaseBin."
}

foreach ($dll in $dllList) {
    $src = Join-Path $releaseBin $dll
    Copy-Item $src -Destination $variantBin -Force
    if ($dll -ne "libcurl.dll") {
        Copy-Item $src -Destination $runtimeDir -Force
    }
}

if ($cpuOnlyBuild -and $openBlasDll -and (Test-Path $openBlasDll)) {
    $libOpenBlasName = "libopenblas.dll"
    Copy-Item $openBlasDll -Destination (Join-Path $variantBin $libOpenBlasName) -Force
    Copy-Item $openBlasDll -Destination (Join-Path $runtimeDir $libOpenBlasName) -Force
    foreach ($legacy in @((Join-Path $variantBin "openblas.dll"), (Join-Path $runtimeDir "openblas.dll"))) {
        if (Test-Path $legacy) {
            Remove-Item $legacy -Force
        }
    }
}
if (Test-Path $curlDll) {
    Copy-Item $curlDll -Destination $variantBin -Force
    Copy-Item $curlDll -Destination $runtimeDir -Force
}

if ($useVulkan -eq "ON" -and $vulkanDllPath -and (Test-Path $vulkanDllPath)) {
    Copy-Item $vulkanDllPath -Destination $variantBin -Force
    Copy-Item $vulkanDllPath -Destination $runtimeDir -Force
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
