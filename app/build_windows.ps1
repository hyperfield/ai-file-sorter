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
    if (Get-Command ninja -ErrorAction SilentlyContinue) {
        $Generator = "Ninja"
    } else {
        $Generator = "Visual Studio 17 2022"
    }
}

$configureArgs = @("-S", $appDir, "-B", $buildDir)
$configureArgs += @("-G", $Generator)
$configureArgs += "-DCMAKE_TOOLCHAIN_FILE=`"$toolchainFile`""
$configureArgs += "-DVCPKG_TARGET_TRIPLET=x64-windows"
$configureArgs += "-DVCPKG_MANIFEST_DIR=`"$appDir`""

if ($Generator -eq "Ninja") {
    $configureArgs += "-DCMAKE_BUILD_TYPE=$Configuration"
} else {
    $configureArgs += "-A"
    $configureArgs += "x64"
}

Write-Host "Configuring project (generator: $Generator, configuration: $Configuration)..."

Write-Host "`n==== CMake Configure Command ===="
Write-Host "cmake $($configureArgs -join ' ')"
Write-Host "=================================`n"

& cmake @configureArgs
if ($LASTEXITCODE -ne 0) {
    throw "cmake configure failed."
}

$buildArgs = @("--build", $buildDir, "--config", $Configuration)

Write-Host "Building..."
& cmake @buildArgs
if ($LASTEXITCODE -ne 0) {
    throw "cmake build failed."
}

$binDir = Join-Path $appDir "bin"
$outputExe = Join-Path $binDir "aifilesorter.exe"
Write-Host "`nBuild complete. Executable located at: $outputExe"

if (-not $SkipDeploy) {
    if ($IsWindows) {
        $windeploy = Join-Path $VcpkgRoot "installed/x64-windows/tools/Qt6/bin/windeployqt.exe"
        if (Test-Path $windeploy) {
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
