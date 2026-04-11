param(
    [string]$VcpkgRoot,
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [switch]$Clean,
    [string]$Generator,
    [switch]$SkipDeploy,
    [switch]$BuildTests,
    [switch]$RunTests,
    [ValidateRange(1, 512)]
    [int]$Parallel = [System.Environment]::ProcessorCount,
    [ValidateSet("Standard", "MsStore", "Standalone")]
    [string[]]$Variants = @("Standard", "MsStore", "Standalone")
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$appDir = $scriptDir
$llamaDir = Join-Path $appDir "include/external/llama.cpp"
$legacySharedVcpkgInstalledDir = Join-Path $appDir "build-windows\\vcpkg_installed"
$dedicatedSharedVcpkgInstalledDir = Join-Path $appDir "build-windows-vcpkg_installed"
$sharedVcpkgInstalledDir = if (Test-Path $legacySharedVcpkgInstalledDir) {
    $legacySharedVcpkgInstalledDir
} else {
    $dedicatedSharedVcpkgInstalledDir
}

$variantDefinitions = @{
    Standard = [pscustomobject]@{
        Name = "Standard"
        BuildDir = (Join-Path $appDir "build-windows")
        UpdateMode = "AUTO_INSTALL"
        PackageKind = "STANDARD"
        Description = "Auto-install updates"
    }
    MsStore = [pscustomobject]@{
        Name = "MsStore"
        BuildDir = (Join-Path $appDir "build-windows-store")
        UpdateMode = "DISABLED"
        PackageKind = "MSIX"
        Description = "No update checks"
    }
    Standalone = [pscustomobject]@{
        Name = "Standalone"
        BuildDir = (Join-Path $appDir "build-windows-standalone")
        UpdateMode = "NOTIFY_ONLY"
        PackageKind = "STANDALONE"
        Description = "Notification-only updates"
    }
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

function Copy-VcpkgRuntimeDlls {
    param(
        [string[]]$SourceDirectories,
        [string]$Destination
    )

    $copied = @()
    foreach ($dir in $SourceDirectories) {
        if (-not $dir) { continue }
        if (-not (Test-Path $dir)) { continue }

        $dlls = Get-ChildItem -Path $dir -Filter "*.dll" -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -notmatch '^Qt6' }

        foreach ($dll in $dlls) {
            Copy-Item $dll.FullName -Destination $Destination -Force
            $copied += $dll.Name
        }
    }

    return $copied | Sort-Object -Unique
}

function Get-ConfigureArguments {
    param(
        [pscustomobject]$Variant,
        [string]$ToolchainFile,
        [switch]$EnableTests,
        [int]$CMakeMajor,
        [int]$CMakeMinor
    )

    $configureArgs = @("-S", $appDir, "-B", $Variant.BuildDir)
    $configureArgs += @("-G", $Generator)
    $configureArgs += "-DCMAKE_TOOLCHAIN_FILE=$ToolchainFile"
    $configureArgs += "-DVCPKG_TARGET_TRIPLET=x64-windows"
    $configureArgs += "-DVCPKG_MANIFEST_DIR=$appDir"
    $configureArgs += "-DVCPKG_INSTALLED_DIR=$sharedVcpkgInstalledDir"
    $configureArgs += "-DAI_FILE_SORTER_UPDATE_MODE=$($Variant.UpdateMode)"
    $configureArgs += "-DAI_FILE_SORTER_WINDOWS_PACKAGE_KIND=$($Variant.PackageKind)"

    if ($EnableTests) {
        $configureArgs += "-DAI_FILE_SORTER_BUILD_TESTS=ON"
    }

    if ($env:AI_FILE_SORTER_STARTER_CONSOLE) {
        $configureArgs += "-DAI_FILE_SORTER_STARTER_CONSOLE=$($env:AI_FILE_SORTER_STARTER_CONSOLE)"
    }

    if ($CMakeMajor -lt 3 -or ($CMakeMajor -eq 3 -and $CMakeMinor -lt 22)) {
        $cmakeMajorMinor = "$CMakeMajor.$CMakeMinor"
        Write-Warning "Detected CMake $cmakeMajorMinor < 3.22; passing QT_FORCE_MIN_CMAKE_VERSION_FOR_USING_QT=$cmakeMajorMinor to satisfy Qt 6.9 requirements."
        $configureArgs += "-DQT_FORCE_MIN_CMAKE_VERSION_FOR_USING_QT=$cmakeMajorMinor"
    }

    if ($Generator -eq "Ninja" -or $Generator -eq "Ninja Multi-Config") {
        $configureArgs += "-DCMAKE_BUILD_TYPE=$Configuration"
    } else {
        $configureArgs += "-A"
        $configureArgs += "x64"
    }

    return ,$configureArgs
}

function Resolve-OutputExecutable {
    param(
        [string]$BuildDir,
        [string]$ConfigurationName
    )

    $binDir = Join-Path $appDir "bin"
    $buildConfigDir = Join-Path $BuildDir $ConfigurationName
    $outputCandidates = @(
        (Join-Path $buildConfigDir "aifilesorter.exe"),
        (Join-Path $BuildDir "aifilesorter.exe"),
        (Join-Path (Join-Path $binDir $ConfigurationName) "aifilesorter.exe"),
        (Join-Path $binDir "aifilesorter.exe")
    )

    foreach ($candidate in $outputCandidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return $candidate
        }
    }

    Write-Warning "Expected executable was not found in standard locations. Reported path may not exist: $($outputCandidates[0])"
    return $outputCandidates[0]
}

function Stage-BuildOutput {
    param(
        [string]$OutputExe,
        [string]$BuildDir
    )

    $outputDir = Split-Path -Parent $OutputExe

    $pdfiumDll = Join-Path $appDir "..\\external\\pdfium\\windows-x64\\bin\\pdfium.dll"
    $pdfiumDllPath = Resolve-Path -Path $pdfiumDll -ErrorAction SilentlyContinue
    if ($pdfiumDllPath) {
        Copy-Item $pdfiumDllPath.Path -Destination $outputDir -Force
    } else {
        Write-Warning "PDFium DLL not found under external/pdfium/windows-x64/bin. Run app\\scripts\\vendor_doc_deps.ps1 (or app/scripts/vendor_doc_deps.sh) to populate it."
    }

    $precompiledCpuBin = Join-Path $appDir "lib/precompiled/cpu/bin"
    $precompiledCudaBin = Join-Path $appDir "lib/precompiled/cuda/bin"
    $precompiledVulkanBin = Join-Path $appDir "lib/precompiled/vulkan/bin"

    $destWocuda = Join-Path $outputDir "lib/ggml/wocuda"
    $destWcuda = Join-Path $outputDir "lib/ggml/wcuda"
    $destWvulkan = Join-Path $outputDir "lib/ggml/wvulkan"

    foreach ($destDir in @($destWocuda, $destWcuda, $destWvulkan)) {
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

    $mingwRuntimeNames = @("libgomp-1.dll", "libgcc_s_seh-1.dll", "libgfortran-5.dll", "libwinpthread-1.dll", "libquadmath-0.dll")
    $runtimeSearchPaths = @()
    if ($env:OPENBLAS_ROOT) {
        $runtimeSearchPaths += (Join-Path $env:OPENBLAS_ROOT "bin")
    }
    $runtimeSearchPaths += "C:\msys64\mingw64\bin"

    foreach ($dllName in $mingwRuntimeNames) {
        $found = $false
        foreach ($path in $runtimeSearchPaths) {
            if (-not (Test-Path $path)) { continue }
            $candidate = Join-Path $path $dllName
            if (Test-Path $candidate) {
                Copy-Item $candidate -Destination $destWocuda -Force
                Copy-Item $candidate -Destination $outputDir -Force
                $found = $true
                break
            }
        }
        if (-not $found) {
            Write-Warning "Could not locate $dllName in any runtime path. Add it manually to $destWocuda if needed."
        }
    }

    if (Test-Path $precompiledCudaBin) {
        Get-ChildItem -Path $precompiledCudaBin -Filter "*.dll" -File -ErrorAction SilentlyContinue |
            ForEach-Object {
                if ($_.Name -ieq "libcurl.dll") { return }
                Copy-Item $_.FullName -Destination $destWcuda -Force
            }
    }

    if (Test-Path $precompiledVulkanBin) {
        Get-ChildItem -Path $precompiledVulkanBin -Filter "*.dll" -File -ErrorAction SilentlyContinue |
            ForEach-Object {
                if ($_.Name -ieq "libcurl.dll") { return }
                Copy-Item $_.FullName -Destination $destWvulkan -Force
            }
    }

    foreach ($destDir in @($destWocuda, $destWcuda, $destWvulkan)) {
        if (Test-Path $destDir) {
            Get-ChildItem -Path $destDir -Filter "*.lib" -File -Recurse -ErrorAction SilentlyContinue |
                Remove-Item -Force
            Get-ChildItem -Path $destDir -Directory -Recurse -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -in @("bin", "lib") } |
                ForEach-Object { Remove-Item $_.FullName -Recurse -Force }
        }
    }

    if (-not $SkipDeploy) {
        $isWindowsHost = [System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([System.Runtime.InteropServices.OSPlatform]::Windows)
        if ($isWindowsHost) {
            $windeployCandidates = @(
                (Join-Path $sharedVcpkgInstalledDir "x64-windows/tools/Qt6/bin/windeployqt.exe"),
                (Join-Path $BuildDir "vcpkg_installed/x64-windows/tools/Qt6/bin/windeployqt.exe"),
                (Join-Path $VcpkgRoot "installed/x64-windows/tools/Qt6/bin/windeployqt.exe"),
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
                Write-Output "Running windeployqt to stage Qt/runtime DLLs for $OutputExe..."
                & $windeploy --no-translations $OutputExe
                if ($LASTEXITCODE -ne 0) {
                    throw "windeployqt failed with exit code $LASTEXITCODE"
                }
            } else {
                Write-Warning "windeployqt.exe not found under vcpkg install roots. Install qtbase via vcpkg or run windeployqt manually."
            }
        } else {
            Write-Warning "Skipping runtime deployment; windeployqt is only available on Windows."
        }
    } else {
        Write-Output "Skipping windeployqt step (per -SkipDeploy)."
    }

    $vcpkgRuntimeSources = @(
        (Join-Path $sharedVcpkgInstalledDir "x64-windows/bin"),
        (Join-Path $BuildDir "vcpkg_installed/x64-windows/bin"),
        (Join-Path $VcpkgRoot "installed/x64-windows/bin")
    )
    $copiedVcpkgDlls = Copy-VcpkgRuntimeDlls -SourceDirectories $vcpkgRuntimeSources -Destination $outputDir
    if ($copiedVcpkgDlls.Count -gt 0) {
        Write-Output ("Staged vcpkg runtime DLLs to {0}:" -f $outputDir)
        Write-Output "  $($copiedVcpkgDlls -join ', ')"
    } else {
        Write-Warning "No vcpkg runtime DLLs were copied; ensure curl/openssl/sqlite runtimes are present beside the executable before distributing."
    }
}

if (-not (Test-Path (Join-Path $llamaDir "CMakeLists.txt"))) {
    throw "llama.cpp submodule not found. Run 'git submodule update --init --recursive' before building."
}

if ($RunTests) {
    $BuildTests = $true
}

$selectedVariantNames = @($Variants | Select-Object -Unique)
$selectedVariants = foreach ($variantName in $selectedVariantNames) {
    $variantDefinitions[$variantName]
}

if ($BuildTests -and ($selectedVariantNames -notcontains "Standard")) {
    throw "-BuildTests and -RunTests currently require the Standard variant because tests are only configured in the auto-update build."
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
    $cmakeMajor = 0
    $cmakeMinor = 0
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

if (-not $Generator) {
    $Generator = "Visual Studio 17 2022"
}

if ($Generator -eq "Ninja" -or $Generator -eq "Ninja Multi-Config") {
    $ninjaEnvArch = $env:VSCMD_ARG_TGT_ARCH
    if ($ninjaEnvArch -and ($ninjaEnvArch -ne "x64")) {
        Write-Warning "Ninja generator selected while MSVC environment targets '$ninjaEnvArch'. Qt packages are built for x64; run from an x64 Native Tools prompt or choose -Generator ""Visual Studio 17 2022""."
    } elseif (-not $ninjaEnvArch) {
        Write-Warning "Using Ninja generator without an initialized MSVC environment. Ensure you run from an x64 Native Tools command prompt so the 64-bit compiler is available."
    }
}

if ($Parallel -lt 1) {
    $Parallel = [Math]::Max([System.Environment]::ProcessorCount, 1)
}

if ($Clean) {
    foreach ($variant in $selectedVariants) {
        if (Test-Path $variant.BuildDir) {
            Write-Output "Removing existing build directory '$($variant.BuildDir)'..."
            Remove-Item -Recurse -Force $variant.BuildDir
        }
    }
    if (Test-Path $sharedVcpkgInstalledDir) {
        Write-Output "Removing shared vcpkg install directory '$sharedVcpkgInstalledDir'..."
        Remove-Item -Recurse -Force $sharedVcpkgInstalledDir
    }
}

if (-not (Test-Path $sharedVcpkgInstalledDir)) {
    New-Item -ItemType Directory -Path $sharedVcpkgInstalledDir | Out-Null
}

Write-Output "Using $Parallel parallel job(s) for builds."
Write-Output "Shared vcpkg installed directory: $sharedVcpkgInstalledDir"

$builtOutputs = New-Object System.Collections.Generic.List[object]

foreach ($variant in $selectedVariants) {
    if (-not (Test-Path $variant.BuildDir)) {
        New-Item -ItemType Directory -Path $variant.BuildDir | Out-Null
    }

    $enableTests = $BuildTests -and $variant.Name -eq "Standard"
    $configureArgs = Get-ConfigureArguments -Variant $variant `
                                            -ToolchainFile $toolchainFile `
                                            -EnableTests:$enableTests `
                                            -CMakeMajor $cmakeMajor `
                                            -CMakeMinor $cmakeMinor

    Write-Output ""
    Write-Output "==== Building $($variant.Name) Variant ===="
    Write-Output "Description : $($variant.Description)"
    Write-Output "Build dir   : $($variant.BuildDir)"
    Write-Output "Update mode : $($variant.UpdateMode)"
    Write-Output "Pkg kind    : $($variant.PackageKind)"
    Write-Output "Configure   : cmake $($configureArgs -join ' ')"
    Write-Output "====================================="

    & $cmakeExe @configureArgs
    if ($LASTEXITCODE -ne 0) {
        throw "cmake configure failed for the $($variant.Name) variant."
    }

    $buildArgs = @("--build", $variant.BuildDir, "--config", $Configuration, "--parallel", $Parallel)
    Write-Output "Building $($variant.Name) variant..."
    & $cmakeExe @buildArgs
    if ($LASTEXITCODE -ne 0) {
        throw "cmake build failed for the $($variant.Name) variant."
    }

    if ($enableTests) {
        Write-Output "Building unit tests in the Standard variant..."
        $testBuildArgs = @("--build", $variant.BuildDir, "--config", $Configuration, "--target", "ai_file_sorter_tests", "--parallel", $Parallel)
        & $cmakeExe @testBuildArgs
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to build unit tests in the Standard variant."
        }

        if ($RunTests) {
            $ctestExe = Join-Path (Split-Path $cmakeExe) "ctest.exe"
            if (-not (Test-Path $ctestExe)) {
                $ctestExe = "ctest"
            }

            Push-Location $variant.BuildDir
            try {
                Write-Output "Running ctest in the Standard variant..."
                & $ctestExe "-C" $Configuration "--output-on-failure" "-j" $Parallel
                if ($LASTEXITCODE -ne 0) {
                    throw "ctest reported failures."
                }
            } finally {
                Pop-Location
            }
        }
    }

    $outputExe = Resolve-OutputExecutable -BuildDir $variant.BuildDir -ConfigurationName $Configuration
    Write-Output "Executable located at: $outputExe"
    Stage-BuildOutput -OutputExe $outputExe -BuildDir $variant.BuildDir

    $builtOutputs.Add([pscustomobject]@{
        Variant = $variant.Name
        UpdateMode = $variant.UpdateMode
        PackageKind = $variant.PackageKind
        Executable = $outputExe
    }) | Out-Null
}

Write-Output ""
Write-Output "Build summary:"
foreach ($output in $builtOutputs) {
    Write-Output (" - {0} [{1}, {2}]: {3}" -f $output.Variant, $output.UpdateMode, $output.PackageKind, $output.Executable)
}
