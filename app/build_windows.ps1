param(
    [string]$VcpkgRoot,
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [switch]$Clean,
    [string]$Generator,
    [switch]$SkipDeploy
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$appDir = $scriptDir
$buildDir = Join-Path $appDir "build-windows"
$llamaDir = Join-Path $appDir "include/external/llama.cpp"

if (-not (Test-Path (Join-Path $llamaDir "CMakeLists.txt"))) {
    throw "llama.cpp submodule not found. Run 'git submodule update --init --recursive' before building."
}

function Resolve-VcpkgRootFromPath {
    param([string]$Path)

    if (-not $Path) { return $null }

    try {
        $candidate = (Resolve-Path $Path -ErrorAction Stop).Path
    } catch {
        return $null
    }

    if ((Get-Item $candidate).PSIsContainer) {
        $dir = $candidate
    } else {
        $dir = (Get-Item $candidate).Directory.FullName
    }

    while ($dir -and (Test-Path $dir)) {
        $toolchain = Join-Path $dir "scripts/buildsystems/vcpkg.cmake"
        if (Test-Path $toolchain) {
            return $dir
        }

        $parent = Split-Path -Parent $dir
        if (-not $parent -or $parent -eq $dir) {
            break
        }
        $dir = $parent
    }

    return $null
}

if (-not $VcpkgRoot) {
    $envCandidates = @($env:VCPKG_ROOT, $env:VPKG_ROOT)
    foreach ($envCandidate in $envCandidates) {
        $resolved = Resolve-VcpkgRootFromPath -Path $envCandidate
        if ($resolved) {
            $VcpkgRoot = $resolved
            break
        }
    }
}

if (-not $VcpkgRoot) {
    $commandCandidates = @("vcpkg", "vpkg")
    foreach ($candidate in $commandCandidates) {
        $cmd = Get-Command $candidate -ErrorAction SilentlyContinue
        if (-not $cmd) { continue }

        $possiblePaths = @($cmd.Source, $cmd.Path, $cmd.Definition)
        foreach ($cPath in $possiblePaths) {
            $resolved = Resolve-VcpkgRootFromPath -Path $cPath
            if ($resolved) {
                $VcpkgRoot = $resolved
                break
            }
        }

        if ($VcpkgRoot) { break }
    }
}

if (-not $VcpkgRoot) {
    throw "Could not locate vcpkg. Provide -VcpkgRoot or set the VCPKG_ROOT environment variable. If vcpkg is installed via winget, pass -VcpkgRoot explicitly (e.g. C:\dev\vcpkg)."
}

$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmakeCommand) {
    throw "cmake executable not found in PATH. Install CMake (3.22+) or add it to PATH."
}
$cmakeExe = $cmakeCommand.Path

$cmakeVersionOutput = & $cmakeExe --version
$cmakeVersionPattern = [regex]'cmake version (?<major>\d+)\.(?<minor>\d+)(\.(?<patch>\d+))?'
$cmakeVersionMatch = $cmakeVersionPattern.Match($cmakeVersionOutput)
if (-not $cmakeVersionMatch.Success) {
    Write-Warning "Unable to parse CMake version from output:`n$cmakeVersionOutput"
} else {
    $cmakeMajor = [int]$cmakeVersionMatch.Groups['major'].Value
    $cmakeMinor = [int]$cmakeVersionMatch.Groups['minor'].Value
    if ($cmakeMajor -lt 3 -or ($cmakeMajor -eq 3 -and $cmakeMinor -lt 16)) {
        throw "CMake 3.16 or newer is required. Detected version $($cmakeVersionMatch.Value)."
    }
}

$toolchainFile = Join-Path $VcpkgRoot "scripts/buildsystems/vcpkg.cmake"
if (-not (Test-Path $toolchainFile)) {
    throw "The provided vcpkg root '$VcpkgRoot' does not contain scripts/buildsystems/vcpkg.cmake."
}

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "Removing existing build directory '$buildDir'..."
    Remove-Item -Recurse -Force $buildDir
}

if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

if (-not $Generator) {
    $Generator = "Visual Studio 17 2022"
}

if ($Generator -eq "Ninja" -or $Generator -eq "Ninja Multi-Config") {
    $ninjaEnvArch = $env:VSCMD_ARG_TGT_ARCH
    if ($ninjaEnvArch -and ($ninjaEnvArch -ne "x64")) {
        Write-Warning "Ninja generator selected while MSVC environment targets '$ninjaEnvArch'. Qt packages are built for x64; run from an x64 Native Tools prompt or choose -Generator \"Visual Studio 17 2022\"."
    } elseif (-not $ninjaEnvArch) {
        Write-Warning "Using Ninja generator without an initialized MSVC environment. Ensure you run from an x64 Native Tools command prompt so the 64-bit compiler is available."
    }
}

$configureArgs = @("-S", $appDir, "-B", $buildDir)
$configureArgs += @("-G", $Generator)
$configureArgs += "-DCMAKE_TOOLCHAIN_FILE=`"$toolchainFile`""
$configureArgs += "-DVCPKG_TARGET_TRIPLET=x64-windows"
$configureArgs += "-DVCPKG_MANIFEST_DIR=`"$appDir`""

if ($cmakeVersionMatch.Success) {
    $cmakeMajorMinor = "$cmakeMajor.$cmakeMinor"
    if ($cmakeMajor -lt 3 -or ($cmakeMajor -eq 3 -and $cmakeMinor -lt 22)) {
        Write-Warning "Detected CMake $cmakeMajorMinor < 3.22; passing QT_FORCE_MIN_CMAKE_VERSION_FOR_USING_QT=$cmakeMajorMinor to satisfy Qt 6.9 requirements."
        $configureArgs += "-DQT_FORCE_MIN_CMAKE_VERSION_FOR_USING_QT=$cmakeMajorMinor"
    }
}

if ($Generator -eq "Ninja" -or $Generator -eq "Ninja Multi-Config") {
    $configureArgs += "-DCMAKE_BUILD_TYPE=$Configuration"
} else {
    $configureArgs += "-A"
    $configureArgs += "x64"
}

Write-Host "Configuring project (generator: $Generator, configuration: $Configuration)..."

Write-Host "`n==== CMake Configure Command ===="
Write-Host "cmake $($configureArgs -join ' ')"
Write-Host "=================================`n"

& $cmakeExe @configureArgs
if ($LASTEXITCODE -ne 0) {
    throw "cmake configure failed."
}

$buildArgs = @("--build", $buildDir, "--config", $Configuration)

Write-Host "Building..."
& $cmakeExe @buildArgs
if ($LASTEXITCODE -ne 0) {
    throw "cmake build failed."
}

$binDir = Join-Path $appDir "bin"
$buildConfigDir = Join-Path $buildDir $Configuration
$outputCandidates = @(
    (Join-Path $buildConfigDir "aifilesorter.exe"),
    (Join-Path $buildDir "aifilesorter.exe"),
    (Join-Path (Join-Path $binDir $Configuration) "aifilesorter.exe"),
    (Join-Path $binDir "aifilesorter.exe")
)
$outputExe = $null
foreach ($candidate in $outputCandidates) {
    if ($candidate -and (Test-Path $candidate)) {
        $outputExe = $candidate
        break
    }
}
if (-not $outputExe) {
    $outputExe = $outputCandidates[0]
    Write-Warning "Expected executable was not found in standard locations. Reported path may not exist: $outputExe"
}

Write-Host "`nBuild complete. Executable located at: $outputExe"

$outputDir = Split-Path -Parent $outputExe
$precompiledCpuBin = Join-Path $appDir "lib/precompiled/cpu/bin"
$precompiledCudaBin = Join-Path $appDir "lib/precompiled/cuda/bin"
$precompiledLibOpenBlas = Join-Path $precompiledCpuBin "libopenblas.dll"
$precompiledOpenBlas = Join-Path $precompiledCpuBin "openblas.dll"

$destWocuda = Join-Path $outputDir "lib/ggml/wocuda"
$destWcuda = Join-Path $outputDir "lib/ggml/wcuda"

foreach ($destDir in @($destWocuda, $destWcuda)) {
    if (-not (Test-Path $destDir)) {
        New-Item -ItemType Directory -Path $destDir -Force | Out-Null
    }
}

if (Test-Path $precompiledCpuBin) {
    Get-ChildItem -Path $precompiledCpuBin -Filter "*.dll" -File -ErrorAction SilentlyContinue |
        ForEach-Object {
            if ($_.Name -ieq "libcurl.dll") { return }
            Copy-Item $_.FullName -Destination $destWocuda -Force
        }
}
if (Test-Path $precompiledCudaBin) {
    Get-ChildItem -Path $precompiledCudaBin -Filter "*.dll" -File -ErrorAction SilentlyContinue |
        ForEach-Object {
            if ($_.Name -ieq "libcurl.dll") { return }
            Copy-Item $_.FullName -Destination $destWcuda -Force
        }
}

foreach ($destDir in @($destWocuda, $destWcuda)) {
    if (Test-Path $destDir) {
        Get-ChildItem -Path $destDir -Filter "*.lib" -File -Recurse -ErrorAction SilentlyContinue |
            Remove-Item -Force
        Get-ChildItem -Path $destDir -Directory -Recurse -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -in @("bin", "lib") } |
            ForEach-Object { Remove-Item $_.FullName -Recurse -Force }
    }
}

# if ((Test-Path $precompiledLibOpenBlas) -or (Test-Path $precompiledOpenBlas)) {
#     $sourceLib = if (Test-Path $precompiledLibOpenBlas) { $precompiledLibOpenBlas } else { $precompiledOpenBlas }
#     $destLibOpen = Join-Path $outputDir "openblas.dll"
#     $destLibPrefixed = Join-Path $outputDir "libopenblas.dll"
#     Write-Host "Staging OpenBLAS runtime from $sourceLib to $outputDir"
#     Copy-Item $sourceLib -Destination $destLibOpen -Force
#     Copy-Item $sourceLib -Destination $destLibPrefixed -Force
# } else {
#     Write-Warning "Could not find libopenblas/openblas in $precompiledCpuBin. Run app/scripts/build_llama_windows.ps1 to regenerate GGML artifacts."
# }

$startAppSrc = Join-Path $buildConfigDir "StartAiFileSorter.exe"
if (-not (Test-Path $startAppSrc)) {
    $startAppSrc = Join-Path $buildDir "StartAiFileSorter.exe"
}
$startAppDest = Join-Path $outputDir "StartAiFileSorter.exe"
if (-not $SkipDeploy) {
    $isWindowsHost = [System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([System.Runtime.InteropServices.OSPlatform]::Windows)
    if ($isWindowsHost) {
        $windeployCandidates = @(
            (Join-Path $VcpkgRoot "installed/x64-windows/tools/Qt6/bin/windeployqt.exe"),
            (Join-Path $buildDir "vcpkg_installed/x64-windows/tools/Qt6/bin/windeployqt.exe"),
            (Join-Path $appDir "vcpkg_installed/x64-windows/tools/Qt6/bin/windeployqt.exe")
        )
        $windeploy = $null
        foreach ($candidate in $windeployCandidates) {
            if ($candidate -and (Test-Path $candidate)) {
                $windeploy = $candidate
                break
            }
        }
        if ($windeploy) {
            Write-Host "Running windeployqt to stage Qt/runtime DLLs..."
            & $windeploy --no-translations "${outputExe}"
            if ($LASTEXITCODE -ne 0) {
                throw "windeployqt failed with exit code $LASTEXITCODE"
            }
        } else {
            Write-Warning "windeployqt.exe not found under $VcpkgRoot. Install qtbase via vcpkg or run windeployqt manually."
        }
    } else {
        Write-Warning "Skipping runtime deployment; windeployqt is only available on Windows."
    }
} else {
    Write-Host "Skipping windeployqt step (per -SkipDeploy)."
}
