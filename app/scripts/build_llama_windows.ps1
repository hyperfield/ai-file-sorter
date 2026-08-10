$ErrorActionPreference = "Stop"

# --- Parse optional arguments ---
$useCuda = "OFF"
$useVulkan = "OFF"
$useBlas = "AUTO" # AUTO = enable BLAS for CPU-only builds by default
$vcpkgRootArg = $null
$openBlasRootArg = $null
$cudaArchArg = $null
foreach ($arg in $args) {
    if ($arg -match "^cuda=(on|off)$") {
        $useCuda = $Matches[1].ToUpper()
    } elseif ($arg -match "^vulkan=(on|off)$") {
        $useVulkan = $Matches[1].ToUpper()
    } elseif ($arg -match "^blas=(on|off)$") {
        $useBlas = $Matches[1].ToUpper()
    } elseif ($arg -match "^vcpkgroot=(.+)$") {
        $vcpkgRootArg = $Matches[1]
    } elseif ($arg -match "^openblasroot=(.+)$") {
        $openBlasRootArg = $Matches[1]
    } elseif ($arg -match "^cudaarch=(.+)$") {
        $cudaArchArg = $Matches[1]
    }
}

if ($useCuda -eq "ON" -and $useVulkan -eq "ON") {
    throw "Cannot enable both CUDA and Vulkan simultaneously. Choose only one backend."
}

Write-Output "`nCUDA Support: $useCuda`n"
Write-Output "Vulkan Support: $useVulkan`n"
Write-Output "BLAS Support: $useBlas (AUTO enables for CPU-only builds)`n"
if ($cudaArchArg) {
    Write-Output "CUDA Architectures Override: $cudaArchArg`n"
}

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

function Get-VsWherePath {
    $candidates = @(
        "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe",
        "C:\Program Files\Microsoft Visual Studio\Installer\vswhere.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    return $null
}

function Get-VisualStudioInstances {
    $vswherePath = Get-VsWherePath
    if (-not $vswherePath) {
        return @()
    }

    $vswhereOutput = & $vswherePath -products * -format json 2>$null
    if ($LASTEXITCODE -ne 0 -or -not $vswhereOutput) {
        return @()
    }

    try {
        $instances = ($vswhereOutput -join [System.Environment]::NewLine) | ConvertFrom-Json -ErrorAction Stop
    } catch {
        Write-Warning "Unable to parse Visual Studio installation metadata from vswhere."
        return @()
    }

    if ($null -eq $instances) {
        return @()
    }

    if ($instances -is [System.Array]) {
        return @($instances)
    }

    return @($instances)
}

function Get-LatestInstalledVisualStudioMajorVersion {
    $instances = Get-VisualStudioInstances |
        Where-Object {
            $_.isComplete -and
            $_.isLaunchable -and
            $_.installationVersion -and
            $_.catalog.productLineVersion -match '^\d+$'
        } |
        Sort-Object -Property @{ Expression = { [version]$_.installationVersion }; Descending = $true }

    $latestInstance = $instances | Select-Object -First 1
    if (-not $latestInstance) {
        return $null
    }

    return [int]$latestInstance.catalog.productLineVersion
}

function Resolve-VisualStudioBundledCMake {
    $instances = Get-VisualStudioInstances |
        Where-Object {
            $_.isComplete -and
            $_.isLaunchable -and
            $_.installationVersion -and
            $_.installationPath
        } |
        Sort-Object -Property @{ Expression = { [version]$_.installationVersion }; Descending = $true }

    foreach ($instance in $instances) {
        $candidate = Join-Path $instance.installationPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    return $null
}

function Get-AvailableCMakeGenerators {
    param([string]$CMakeExecutable)

    if (-not $CMakeExecutable -or -not (Test-Path $CMakeExecutable)) {
        return @()
    }

    $capabilitiesOutput = & $CMakeExecutable -E capabilities 2>$null
    if ($LASTEXITCODE -ne 0 -or -not $capabilitiesOutput) {
        return @()
    }

    try {
        $capabilities = ($capabilitiesOutput -join [System.Environment]::NewLine) | ConvertFrom-Json -ErrorAction Stop
    } catch {
        Write-Warning "Unable to parse generator capabilities from '$CMakeExecutable'."
        return @()
    }

    if (-not $capabilities.generators) {
        return @()
    }

    return @(
        $capabilities.generators |
            Where-Object { $_.name } |
            Select-Object -ExpandProperty name
    )
}

function Test-GeneratorsContainVisualStudioMajor {
    param(
        [string[]]$GeneratorNames,
        [Nullable[Int32]]$MajorVersion
    )

    if ($null -eq $MajorVersion) {
        return $false
    }

    foreach ($generatorName in $GeneratorNames) {
        if ($generatorName -match '^Visual Studio (?<major>\d+) \d{4}$' -and [int]$matches['major'] -eq $MajorVersion) {
            return $true
        }
    }

    return $false
}

function Resolve-DefaultVisualStudioGenerator {
    param(
        [string[]]$AvailableGenerators,
        [Nullable[Int32]]$PreferredVisualStudioMajor
    )

    $visualStudioGenerators = @(
        foreach ($generatorName in $AvailableGenerators) {
            if ($generatorName -match '^Visual Studio (?<major>\d+) (?<year>\d{4})$') {
                [pscustomobject]@{
                    Name = $generatorName
                    Major = [int]$matches['major']
                    Year = [int]$matches['year']
                }
            }
        }
    )

    if ($null -ne $PreferredVisualStudioMajor) {
        $preferredGenerator = $visualStudioGenerators |
            Where-Object { $_.Major -eq $PreferredVisualStudioMajor } |
            Sort-Object -Property @{ Expression = { $_.Year }; Descending = $true } |
            Select-Object -First 1
        if ($preferredGenerator) {
            return $preferredGenerator.Name
        }
    }

    $latestGenerator = $visualStudioGenerators |
        Sort-Object -Property @{ Expression = { $_.Major }; Descending = $true }, @{ Expression = { $_.Year }; Descending = $true } |
        Select-Object -First 1
    if ($latestGenerator) {
        return $latestGenerator.Name
    }

    return "Visual Studio 17 2022"
}

function Test-NormalizedPathEqual {
    param(
        [string]$Left,
        [string]$Right
    )

    if (-not $Left -or -not $Right) {
        return $false
    }

    $normalizedLeft = [System.IO.Path]::GetFullPath($Left).TrimEnd('\')
    $normalizedRight = [System.IO.Path]::GetFullPath($Right).TrimEnd('\')
    return $normalizedLeft.Equals($normalizedRight, [System.StringComparison]::OrdinalIgnoreCase)
}

# --- Locate cmake executable ---
function Resolve-CMake {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }
    $vsCMake = Resolve-VisualStudioBundledCMake
    if (Test-Path $vsCMake) {
        return $vsCMake
    }
    throw "cmake executable not found in PATH. Run this script from a VS Developer PowerShell or install CMake and ensure it is on PATH."
}

$cmakeExe = Resolve-CMake
$availableGenerators = Get-AvailableCMakeGenerators -CMakeExecutable $cmakeExe
$latestVisualStudioMajor = Get-LatestInstalledVisualStudioMajorVersion
if (-not (Test-GeneratorsContainVisualStudioMajor -GeneratorNames $availableGenerators -MajorVersion $latestVisualStudioMajor)) {
    $bundledCMakeExe = Resolve-VisualStudioBundledCMake
    if ($bundledCMakeExe -and -not (Test-NormalizedPathEqual -Left $cmakeExe -Right $bundledCMakeExe)) {
        $bundledGenerators = Get-AvailableCMakeGenerators -CMakeExecutable $bundledCMakeExe
        if (Test-GeneratorsContainVisualStudioMajor -GeneratorNames $bundledGenerators -MajorVersion $latestVisualStudioMajor) {
            Write-Output "Switching to Visual Studio-bundled CMake because '$cmakeExe' does not support the installed Visual Studio $latestVisualStudioMajor generator."
            $cmakeExe = $bundledCMakeExe
            $availableGenerators = $bundledGenerators
        }
    }
}
$visualStudioGenerator = Resolve-DefaultVisualStudioGenerator -AvailableGenerators $availableGenerators -PreferredVisualStudioMajor $latestVisualStudioMajor

function Resolve-MSVCCompiler {
    $cmd = Get-Command cl.exe -ErrorAction SilentlyContinue
    if ($cmd -and (Test-Path $cmd.Source)) {
        return $cmd.Source
    }

    $vswherePath = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswherePath) {
        $installationPath = & $vswherePath `
            -latest `
            -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath

        if ($installationPath) {
            $toolRoot = Join-Path $installationPath "VC\Tools\MSVC"
            if (Test-Path $toolRoot) {
                $toolsets = Get-ChildItem -Path $toolRoot -Directory |
                    Sort-Object Name -Descending
                foreach ($toolset in $toolsets) {
                    $candidate = Join-Path $toolset.FullName "bin\Hostx64\x64\cl.exe"
                    if (Test-Path $candidate) {
                        return $candidate
                    }
                }
            }
        }
    }

    throw "Could not locate MSVC cl.exe. Run this script from a Developer PowerShell or install the Visual Studio C++ toolchain."
}

$msvcCompiler = Resolve-MSVCCompiler

# --- Locate OpenBLAS (required on Windows) ---
function Resolve-VcpkgRootFromPath {
    param([string]$Path)

    if (-not $Path) {
        return $null
    }

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
        $toolchain = Join-Path $dir "scripts\buildsystems\vcpkg.cmake"
        $exe = Join-Path $dir "vcpkg.exe"
        if ((Test-Path $toolchain) -and (Test-Path $exe)) {
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

function Test-VcpkgRootWritable {
    param([string]$Root)

    if (-not $Root -or -not (Test-Path $Root)) {
        return $false
    }

    try {
        $probePath = Join-Path $Root ".aifs-vcpkg-write-probe.tmp"
        Set-Content -LiteralPath $probePath -Value "probe" -NoNewline
        Remove-Item -LiteralPath $probePath -Force
        return $true
    } catch {
        return $false
    }
}

function Test-IsVisualStudioBundledVcpkgRoot {
    param([string]$Root)

    if (-not $Root) {
        return $false
    }

    return $Root -like "*Program Files*Microsoft Visual Studio*\VC\vcpkg"
}

function Get-DefaultVcpkgRootCandidates {
    $candidates = New-Object System.Collections.Generic.List[string]

    $repoDrive = [System.IO.Path]::GetPathRoot([System.IO.Path]::GetFullPath($scriptDir)).TrimEnd('\')
    $systemDrive = if ($env:SystemDrive) { $env:SystemDrive.TrimEnd('\') } else { $null }

    $preferredRoots = @()
    if ($repoDrive) { $preferredRoots += $repoDrive }
    if ($systemDrive -and $systemDrive -ne $repoDrive) { $preferredRoots += $systemDrive }

    $otherRoots = Get-PSDrive -PSProvider FileSystem -ErrorAction SilentlyContinue |
        Select-Object -ExpandProperty Root |
        ForEach-Object { $_.TrimEnd('\') } |
        Where-Object { $_ -match '^[A-Za-z]:$' } |
        Where-Object { $_ -and ($_ -notin $preferredRoots) } |
        Sort-Object

    foreach ($root in @($preferredRoots + $otherRoots)) {
        $candidates.Add((Join-Path $root "dev\vcpkg"))
        $candidates.Add((Join-Path $root "vcpkg"))
    }

    $repoRoot = [System.IO.Path]::GetFullPath((Join-Path $scriptDir "..\.."))
    $repoParent = Split-Path -Parent $repoRoot
    if ($repoParent) {
        $candidates.Add((Join-Path $repoParent "vcpkg"))
    }

    return $candidates | Select-Object -Unique
}

function Resolve-VcpkgRoot {
    param([string]$Explicit)

    if ($Explicit) {
        $resolvedExplicit = Resolve-VcpkgRootFromPath -Path $Explicit
        if (-not $resolvedExplicit) {
            throw "The provided vcpkg root '$Explicit' does not contain vcpkg.exe and scripts\buildsystems\vcpkg.cmake."
        }
        if (-not (Test-VcpkgRootWritable -Root $resolvedExplicit)) {
            throw "The provided vcpkg root '$resolvedExplicit' is not writable. Choose a writable clone outside protected locations such as Program Files."
        }
        return $resolvedExplicit
    }

    $rejectedCandidates = New-Object System.Collections.Generic.List[string]
    $candidateSources = @()

    foreach ($envCandidate in @($env:VCPKG_ROOT, $env:VPKG_ROOT)) {
        if ($envCandidate) {
            $candidateSources += $envCandidate
        }
    }

    foreach ($commandName in @("vcpkg", "vpkg")) {
        $cmd = Get-Command $commandName -ErrorAction SilentlyContinue
        if (-not $cmd) { continue }
        foreach ($pathCandidate in @($cmd.Source, $cmd.Path, $cmd.Definition)) {
            if ($pathCandidate) {
                $candidateSources += $pathCandidate
            }
        }
    }

    $candidateSources += Get-DefaultVcpkgRootCandidates

    foreach ($candidateSource in ($candidateSources | Select-Object -Unique)) {
        $resolved = Resolve-VcpkgRootFromPath -Path $candidateSource
        if (-not $resolved) {
            continue
        }
        if (Test-IsVisualStudioBundledVcpkgRoot -Root $resolved) {
            $rejectedCandidates.Add("Skipped Visual Studio bundled vcpkg at '$resolved' because it is usually read-only.") | Out-Null
            continue
        }
        if (-not (Test-VcpkgRootWritable -Root $resolved)) {
            $rejectedCandidates.Add("Skipped non-writable vcpkg root '$resolved'.") | Out-Null
            continue
        }
        return $resolved
    }

    foreach ($message in $rejectedCandidates) {
        Write-Warning $message
    }

    return $null
}

$cpuOnlyBuild = ($useCuda -eq "OFF" -and $useVulkan -eq "OFF")
$enableBlas = ($useBlas -eq "ON") -or ($useBlas -eq "AUTO" -and $cpuOnlyBuild)

function Get-OpenBlasHeaderCandidates {
    param([string]$Root)

    return @(
        (Join-Path $Root "include\openblas\cblas.h"),
        (Join-Path $Root "include\cblas.h")
    )
}

function Get-OpenBlasLibraryCandidates {
    param([string]$Root)

    return @(
        (Join-Path $Root "lib\openblas.lib"),
        (Join-Path $Root "lib\libopenblas.lib"),
        (Join-Path $Root "lib\libopenblas.dll.a"),
        (Join-Path $Root "lib\libopenblas.a")
    )
}

function Get-OpenBlasDllCandidates {
    param([string]$Root)

    return @(
        (Join-Path $Root "bin\libopenblas.dll"),
        (Join-Path $Root "bin\openblas.dll")
    )
}

function Resolve-OpenBlasIncludeDirectory {
    param([string]$Root)

    $nestedIncludeDir = Join-Path $Root "include\openblas"
    if (Test-Path (Join-Path $nestedIncludeDir "cblas.h")) {
        return $nestedIncludeDir
    }

    $flatIncludeDir = Join-Path $Root "include"
    if (Test-Path (Join-Path $flatIncludeDir "cblas.h")) {
        return $flatIncludeDir
    }

    return $nestedIncludeDir
}

function Test-OpenBlasRootCandidate {
    param([string]$Root)

    if (-not $Root -or -not (Test-Path $Root)) {
        return $false
    }

    $hasHeader = $false
    foreach ($candidate in (Get-OpenBlasHeaderCandidates -Root $Root)) {
        if (Test-Path $candidate) {
            $hasHeader = $true
            break
        }
    }

    if (-not $hasHeader) {
        return $false
    }

    $hasLibrary = $false
    foreach ($candidate in (Get-OpenBlasLibraryCandidates -Root $Root)) {
        if (Test-Path $candidate) {
            $hasLibrary = $true
            break
        }
    }

    if (-not $hasLibrary) {
        return $false
    }

    foreach ($candidate in (Get-OpenBlasDllCandidates -Root $Root)) {
        if (Test-Path $candidate) {
            return $true
        }
    }

    return $false
}

function Resolve-OpenBlasRoot {
    param([string]$Explicit)

    $candidates = @()
    if ($Explicit) { $candidates += $Explicit }
    if ($env:OPENBLAS_ROOT) { $candidates += $env:OPENBLAS_ROOT }
    $candidates += "C:\msys64\ucrt64"
    $candidates += "C:\msys64\mingw64"

    foreach ($candidate in ($candidates | Select-Object -Unique)) {
        if (-not $candidate -or -not (Test-Path $candidate)) {
            continue
        }

        $candidateVariants = @($candidate)
        if ((Split-Path $candidate -Leaf) -ieq "msys64") {
            $candidateVariants += @(
                (Join-Path $candidate "ucrt64"),
                (Join-Path $candidate "mingw64")
            )
        }

        foreach ($variant in ($candidateVariants | Select-Object -Unique)) {
            if (Test-OpenBlasRootCandidate -Root $variant) {
                return (Resolve-Path $variant).Path
            }
        }
    }
    return $null
}

function Test-CudaToolkitRoot {
    param([string]$Root)

    if (-not $Root) {
        return $false
    }

    $cudaHeader = Join-Path $Root "include\cuda.h"
    $cudartLib = Join-Path $Root "lib\x64\cudart.lib"
    return (Test-Path $cudaHeader) -and (Test-Path $cudartLib)
}

function Get-CudaToolkitVersion {
    param([string]$Path)

    if (-not $Path) {
        return [version]"0.0"
    }

    $name = Split-Path $Path -Leaf
    $normalized = $name.TrimStart('v', 'V')
    try {
        return [version]$normalized
    } catch {
        return [version]"0.0"
    }
}

function Get-NvidiaDriverCudaVersion {
    $nvidiaSmi = Get-Command nvidia-smi.exe -ErrorAction SilentlyContinue
    if (-not $nvidiaSmi -or -not (Test-Path $nvidiaSmi.Source)) {
        $defaultNvidiaSmi = "C:\Program Files\NVIDIA Corporation\NVSMI\nvidia-smi.exe"
        if (Test-Path $defaultNvidiaSmi) {
            $nvidiaSmi = [pscustomobject]@{ Source = $defaultNvidiaSmi }
        }
    }

    if (-not $nvidiaSmi -or -not (Test-Path $nvidiaSmi.Source)) {
        return $null
    }

    try {
        $output = (& $nvidiaSmi.Source 2>$null) | Out-String
        if ($output -match 'CUDA Version:\s*([0-9]+(?:\.[0-9]+)?)') {
            return [version]$Matches[1]
        }
    } catch {
        return $null
    }

    return $null
}

function Resolve-CudaRoot {
    $candidates = New-Object System.Collections.Generic.List[string]

    if ($env:CUDA_PATH) {
        $candidates.Add($env:CUDA_PATH)
    }

    Get-ChildItem Env:CUDA_PATH_V* -ErrorAction SilentlyContinue |
        Sort-Object { Get-CudaToolkitVersion $_.Value } -Descending |
        ForEach-Object {
            if ($_.Value) {
                $candidates.Add($_.Value)
            }
        }

    $defaultCudaRoot = "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA"
    if (Test-Path $defaultCudaRoot) {
        Get-ChildItem -Path $defaultCudaRoot -Directory |
            Sort-Object { Get-CudaToolkitVersion $_.FullName } -Descending |
            ForEach-Object {
                $candidates.Add($_.FullName)
            }
    }

    $explicitCudaPath = $null
    if ($env:CUDA_PATH) {
        try {
            if (Test-Path $env:CUDA_PATH) {
                $explicitCudaPath = (Resolve-Path $env:CUDA_PATH).Path
            } else {
                $explicitCudaPath = $env:CUDA_PATH
            }
        } catch {
            $explicitCudaPath = $env:CUDA_PATH
        }
    }

    $driverCudaVersion = Get-NvidiaDriverCudaVersion
    $seen = @{}
    $firstUsable = $null
    $skippedNewerToolkits = New-Object System.Collections.Generic.List[string]
    foreach ($candidate in $candidates) {
        if (-not $candidate) {
            continue
        }

        $resolved = $candidate
        try {
            if (Test-Path $candidate) {
                $resolved = (Resolve-Path $candidate).Path
            }
        } catch {
            $resolved = $candidate
        }

        $key = $resolved.ToLowerInvariant()
        if ($seen.ContainsKey($key)) {
            continue
        }
        $seen[$key] = $true

        if (-not (Test-CudaToolkitRoot -Root $resolved)) {
            continue
        }

        $toolkitVersion = Get-CudaToolkitVersion $resolved
        if (-not $firstUsable) {
            $firstUsable = [pscustomobject]@{
                Root = $resolved
                Version = $toolkitVersion
            }
        }

        if ($explicitCudaPath -and $resolved -ieq $explicitCudaPath) {
            if ($driverCudaVersion -and $toolkitVersion -gt $driverCudaVersion) {
                Write-Warning "CUDA_PATH points to toolkit v$toolkitVersion, but the installed NVIDIA driver reports CUDA $driverCudaVersion support. Builds may succeed but fail at runtime with PTX/toolchain errors."
            }
            return $resolved
        }

        if (-not $driverCudaVersion -or $toolkitVersion -le $driverCudaVersion) {
            if ($driverCudaVersion -and $skippedNewerToolkits.Count -gt 0) {
                $skippedList = ($skippedNewerToolkits -join ", ")
                Write-Warning "Skipping newer CUDA toolkit(s) $skippedList because the installed NVIDIA driver reports CUDA $driverCudaVersion support. Using $resolved."
            }
            return $resolved
        }

        $skippedNewerToolkits.Add("v$toolkitVersion")
    }

    if ($firstUsable) {
        if ($driverCudaVersion -and $skippedNewerToolkits.Count -gt 0) {
            $skippedList = ($skippedNewerToolkits -join ", ")
            Write-Warning "No installed CUDA toolkit is <= driver-reported CUDA $driverCudaVersion. Falling back to $($firstUsable.Root) after skipping $skippedList."
        }
        return $firstUsable.Root
    }

    throw "Could not resolve a usable CUDA toolkit. Expected include\cuda.h and lib\x64\cudart.lib under CUDA_PATH or a standard install such as C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\vX.Y."
}

$vcpkgRoot = Resolve-VcpkgRoot -Explicit $vcpkgRootArg
if (-not $vcpkgRoot -or -not (Test-Path $vcpkgRoot)) {
    throw "Could not resolve a writable vcpkg root. Set VCPKG_ROOT, put vcpkg on PATH, or install a writable clone in a common location such as <drive>:\dev\vcpkg or <drive>:\vcpkg."
}
$env:VCPKG_ROOT = $vcpkgRoot
Write-Output "Using vcpkg root: $vcpkgRoot"

function Convert-ToCMakePath {
    param([string]$Path)

    if (-not $Path) {
        return $null
    }

    return ([System.IO.Path]::GetFullPath($Path)).Replace('\', '/')
}

function New-CMakeCacheArg {
    param(
        [string]$Name,
        [string]$Value,
        [string]$Type = $null
    )

    if ([string]::IsNullOrWhiteSpace($Name)) {
        throw "New-CMakeCacheArg requires a non-empty cache variable name."
    }
    if ($null -eq $Value) {
        throw "New-CMakeCacheArg requires a value for '$Name'."
    }

    if ($Type) {
        return "-D${Name}:${Type}=$Value"
    }

    return "-D${Name}=$Value"
}

function Push-TransientGitSafeDirectory {
    param([string]$Directory)

    if (-not $Directory) {
        throw "Push-TransientGitSafeDirectory requires a directory path."
    }

    $state = @{
        OriginalCount = $env:GIT_CONFIG_COUNT
        KeyName = $null
        ValueName = $null
        OriginalKey = $null
        OriginalValue = $null
    }

    $configCount = 0
    if ($env:GIT_CONFIG_COUNT) {
        [void][int]::TryParse($env:GIT_CONFIG_COUNT, [ref]$configCount)
    }

    $keyName = "GIT_CONFIG_KEY_$configCount"
    $valueName = "GIT_CONFIG_VALUE_$configCount"
    $existingKey = Get-Item -Path "Env:$keyName" -ErrorAction SilentlyContinue
    $existingValue = Get-Item -Path "Env:$valueName" -ErrorAction SilentlyContinue

    $state.KeyName = $keyName
    $state.ValueName = $valueName
    $state.OriginalKey = if ($existingKey) { $existingKey.Value } else { $null }
    $state.OriginalValue = if ($existingValue) { $existingValue.Value } else { $null }

    Set-Item -Path "Env:$keyName" -Value "safe.directory"
    Set-Item -Path "Env:$valueName" -Value ([System.IO.Path]::GetFullPath($Directory).Replace('\', '/'))
    $env:GIT_CONFIG_COUNT = ($configCount + 1).ToString()

    return $state
}

function Pop-TransientGitSafeDirectory {
    param($State)

    if (-not $State) {
        return
    }

    if ($State.KeyName) {
        if ($null -ne $State.OriginalKey) {
            Set-Item -Path ("Env:" + $State.KeyName) -Value $State.OriginalKey
        } else {
            Remove-Item -Path ("Env:" + $State.KeyName) -ErrorAction SilentlyContinue
        }
    }

    if ($State.ValueName) {
        if ($null -ne $State.OriginalValue) {
            Set-Item -Path ("Env:" + $State.ValueName) -Value $State.OriginalValue
        } else {
            Remove-Item -Path ("Env:" + $State.ValueName) -ErrorAction SilentlyContinue
        }
    }

    if ($null -ne $State.OriginalCount -and $State.OriginalCount -ne "") {
        $env:GIT_CONFIG_COUNT = $State.OriginalCount
    } else {
        Remove-Item -Path "Env:GIT_CONFIG_COUNT" -ErrorAction SilentlyContinue
    }
}

function Get-LlamaBuildVariantId {
    if ($useCuda -eq "ON") {
        return "cuda"
    }

    if ($useVulkan -eq "ON") {
        if ($enableBlas) {
            return "vulkan-blas"
        }
        return "vulkan"
    }

    if ($enableBlas) {
        return "cpu-blas"
    }

    return "cpu"
}

function Get-LlamaBuildDirectory {
    param([string]$LlamaRoot)

    $driveRoot = [System.IO.Path]::GetPathRoot([System.IO.Path]::GetFullPath($LlamaRoot))
    if (-not $driveRoot) {
        throw "Could not resolve the build drive root for '$LlamaRoot'."
    }

    $variantId = Get-LlamaBuildVariantId
    return Join-Path $driveRoot ("aifs-llama-" + $variantId)
}

function Invoke-DownloadFile {
    param(
        [string]$Uri,
        [string]$Destination
    )

    Invoke-WebRequest -Uri $Uri -OutFile $Destination -UseBasicParsing
}

function Ensure-LlamaUiDist {
    param(
        [string]$LlamaRoot,
        [string]$CMakeExecutable
    )

    $uiSourceDir = Join-Path $LlamaRoot "tools\ui"
    $uiStaticDir = Join-Path $uiSourceDir "static"
    $uiDistDir = Join-Path $uiSourceDir "dist"
    $loadingSource = Join-Path $uiStaticDir "loading.html"
    $indexPath = Join-Path $uiDistDir "index.html"
    $loadingDest = Join-Path $uiDistDir "loading.html"
    $hadUsableDist = (Test-Path $indexPath)

    if ($hadUsableDist -and (Test-Path $loadingDest)) {
        return @{
            DistDir = $uiDistDir
            CleanupAfterBuild = $false
        }
    }

    if ($hadUsableDist) {
        if (-not (Test-Path $loadingSource)) {
            throw "UI assets exist in $uiDistDir, but $loadingSource is missing, so the loading page cannot be staged."
        }

        Copy-Item $loadingSource -Destination $loadingDest -Force
        Write-Output "Added missing UI loading fallback to $uiDistDir"
        return @{
            DistDir = $uiDistDir
            CleanupAfterBuild = $false
        }
    }

    $cacheDir = Join-Path $uiSourceDir ".aifs-prebuilt-ui"
    $archivePath = Join-Path $cacheDir "dist.tar.gz"
    $checksumPath = Join-Path $cacheDir "dist.tar.gz.sha256"
    $archiveUrl = "https://huggingface.co/buckets/ggml-org/llama-ui/resolve/latest/dist.tar.gz?download=true"
    $checksumUrl = "https://huggingface.co/buckets/ggml-org/llama-ui/resolve/latest/dist.tar.gz.sha256?download=true"

    New-Item -ItemType Directory -Force -Path $cacheDir | Out-Null

    Write-Output "Downloading prebuilt llama UI assets from latest HF bucket release"
    Invoke-DownloadFile -Uri $archiveUrl -Destination $archivePath
    Invoke-DownloadFile -Uri $checksumUrl -Destination $checksumPath

    $expectedChecksumContent = Get-Content -LiteralPath $checksumPath -Raw
    if ($expectedChecksumContent -notmatch '^[0-9a-fA-F]+') {
        throw "The UI asset checksum file '$checksumPath' did not contain a valid SHA256 digest."
    }

    $expectedChecksum = $Matches[0].ToLowerInvariant()
    $actualChecksum = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualChecksum -ne $expectedChecksum) {
        throw "Checksum verification failed for '$archivePath'. Expected $expectedChecksum, got $actualChecksum."
    }

    if (Test-Path $uiDistDir) {
        Remove-Item -Recurse -Force $uiDistDir
    }
    New-Item -ItemType Directory -Force -Path $uiDistDir | Out-Null

    Push-Location $uiDistDir
    try {
        & $CMakeExecutable -E tar xzf $archivePath
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to extract prebuilt UI assets from '$archivePath'."
        }
    } finally {
        Pop-Location
    }

    if (-not (Test-Path $indexPath)) {
        throw "The downloaded UI asset archive did not contain index.html after extraction to $uiDistDir."
    }

    if (-not (Test-Path $loadingSource)) {
        throw "The vendored UI static asset '$loadingSource' is missing, so loading.html cannot be staged."
    }

    Copy-Item $loadingSource -Destination $loadingDest -Force

    return @{
        DistDir = $uiDistDir
        CleanupAfterBuild = $true
    }
}

function Get-FirstExistingPath {
    param([string[]]$Candidates)

    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return $candidate
        }
    }

    return $null
}

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

$openSslRoot = Join-Path $vcpkgRoot "installed\$triplet"
$openSslInclude = Join-Path $openSslRoot "include"
$openSslSslLibCandidates = @(
    (Join-Path $openSslRoot "lib\libssl.lib"),
    (Join-Path $openSslRoot "lib\ssl.lib")
)
$openSslCryptoLibCandidates = @(
    (Join-Path $openSslRoot "lib\libcrypto.lib"),
    (Join-Path $openSslRoot "lib\crypto.lib")
)
Confirm-VcpkgPackage `
    -HeaderCheckPath (Join-Path $openSslInclude "openssl\ssl.h") `
    -LibraryCheckPath $null `
    -PackageName "openssl" `
    -AdditionalPaths @($openSslSslLibCandidates[0], $openSslCryptoLibCandidates[0])

$openSslSslLib = Get-FirstExistingPath -Candidates $openSslSslLibCandidates
$openSslCryptoLib = Get-FirstExistingPath -Candidates $openSslCryptoLibCandidates
if (-not $openSslSslLib -or -not $openSslCryptoLib) {
    throw "Could not locate OpenSSL import libraries under $openSslRoot\lib after installing openssl."
}

$openSslRuntimeDlls = @()
$openSslBin = Join-Path $openSslRoot "bin"
if (Test-Path $openSslBin) {
    $openSslRuntimeDlls += @(Get-ChildItem -Path $openSslBin -Filter "libssl*.dll" -File -ErrorAction SilentlyContinue)
    $openSslRuntimeDlls += @(Get-ChildItem -Path $openSslBin -Filter "libcrypto*.dll" -File -ErrorAction SilentlyContinue)
}
if ($openSslRuntimeDlls.Count -eq 0) {
    Write-Warning "OpenSSL runtime DLLs were not found under $openSslBin. HTTPS-enabled llama components may fail at runtime unless those DLLs are otherwise available."
}

$openBlasInclude = $null
$openBlasLib = $null
$openBlasDll = $null
$openBlasCompanionDllNames = @(
    "libgomp-1.dll",
    "libgcc_s_seh-1.dll",
    "libgfortran-5.dll",
    "libwinpthread-1.dll",
    "libquadmath-0.dll"
)
if ($enableBlas) {
    $openBlasRoot = Resolve-OpenBlasRoot -Explicit $openBlasRootArg
    if (-not $openBlasRoot) {
        throw "BLAS builds require an OpenBLAS runtime tree (for example C:\msys64\ucrt64 or C:\msys64\mingw64). Pass openblasroot=<path> or set OPENBLAS_ROOT."
    }

    $openBlasInclude = Resolve-OpenBlasIncludeDirectory -Root $openBlasRoot
    $openBlasHeader = Join-Path $openBlasInclude "cblas.h"
    if (-not (Test-Path $openBlasHeader)) {
        throw "Missing cblas.h under $openBlasInclude. Install OpenBLAS via MSYS2 (for example pacman -S mingw-w64-ucrt-x86_64-openblas or mingw-w64-x86_64-openblas) or point openblasroot to a valid tree."
    }

    $openBlasLibCandidates = Get-OpenBlasLibraryCandidates -Root $openBlasRoot
    foreach ($candidate in $openBlasLibCandidates) {
        if (Test-Path $candidate) {
            $openBlasLib = $candidate
            break
        }
    }
    if (-not $openBlasLib) {
        throw "Could not find an OpenBLAS import library under $openBlasRoot\lib."
    }

    $openBlasDllCandidates = Get-OpenBlasDllCandidates -Root $openBlasRoot
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

$llamaBuildDir = Get-LlamaBuildDirectory -LlamaRoot $llamaDir
$uiDistState = Ensure-LlamaUiDist -LlamaRoot $llamaDir -CMakeExecutable $cmakeExe
$gitSafeDirectoryState = $null

# Build Windows ggml runtimes so they remain compatible with older x64 CPUs
# that support SSE4.2 but not AVX/AVX2/BMI2.
$ggmlWindowsCpuFeatureArgs = @(
    "-DGGML_SSE42=ON",
    "-DGGML_AVX=OFF",
    "-DGGML_AVX_VNNI=OFF",
    "-DGGML_AVX2=OFF",
    "-DGGML_BMI2=OFF",
    "-DGGML_AVX512=OFF",
    "-DGGML_AVX512_VBMI=OFF",
    "-DGGML_AVX512_VNNI=OFF",
    "-DGGML_AVX512_BF16=OFF"
)

$cmakeArgs = @(
    (New-CMakeCacheArg -Name "CMAKE_C_COMPILER" -Type "FILEPATH" -Value (Convert-ToCMakePath $msvcCompiler)),
    (New-CMakeCacheArg -Name "CMAKE_CXX_COMPILER" -Type "FILEPATH" -Value (Convert-ToCMakePath $msvcCompiler)),
    "-DLLAMA_OPENSSL=ON",
    "-DLLAMA_UI_GZIP=OFF",
    (New-CMakeCacheArg -Name "OPENSSL_ROOT_DIR" -Type "PATH" -Value (Convert-ToCMakePath $openSslRoot)),
    (New-CMakeCacheArg -Name "OPENSSL_INCLUDE_DIR" -Type "PATH" -Value (Convert-ToCMakePath $openSslInclude)),
    (New-CMakeCacheArg -Name "OPENSSL_SSL_LIBRARY" -Type "FILEPATH" -Value (Convert-ToCMakePath $openSslSslLib)),
    (New-CMakeCacheArg -Name "OPENSSL_CRYPTO_LIBRARY" -Type "FILEPATH" -Value (Convert-ToCMakePath $openSslCryptoLib)),
    "-DBUILD_SHARED_LIBS=ON",
    "-DGGML_OPENCL=OFF",
    "-DGGML_VULKAN=$useVulkan",
    "-DGGML_SYCL=OFF",
    "-DGGML_HIP=OFF",
    "-DGGML_KLEIDIAI=OFF",
    "-DGGML_NATIVE=OFF",
    "-DCMAKE_CXX_FLAGS=/EHsc"
)
$cmakeArgs += $ggmlWindowsCpuFeatureArgs

if ($enableBlas) {
    if (-not $openBlasInclude -or -not $openBlasLib) {
        throw "OpenBLAS paths not initialized for the BLAS-enabled build."
    }
    $cmakeArgs += @(
        "-DGGML_BLAS=ON",
        "-DGGML_BLAS_VENDOR=OpenBLAS",
        "-DBLA_VENDOR=OpenBLAS",
        (New-CMakeCacheArg -Name "BLAS_INCLUDE_DIRS" -Type "PATH" -Value (Convert-ToCMakePath $openBlasInclude)),
        (New-CMakeCacheArg -Name "BLAS_LIBRARIES" -Type "FILEPATH" -Value (Convert-ToCMakePath $openBlasLib))
    )
} else {
    $cmakeArgs += "-DGGML_BLAS=OFF"
}

if ($useCuda -eq "ON") {
    $cudaRoot = Resolve-CudaRoot
    $libDir = "$cudaRoot/lib/x64/cudart.lib"
    $cudaBinDir = "$cudaRoot/bin"
    $nvccPath = Join-Path $cudaBinDir "nvcc.exe"

    Write-Host "Using CUDA toolkit from $cudaRoot"
    if (Test-Path $cudaBinDir) {
        $env:PATH = "$cudaBinDir;$env:PATH"
    }
    $env:CUDA_PATH = $cudaRoot
    $env:CUDAToolkit_ROOT = $cudaRoot
    $env:CudaToolkitDir = "$cudaRoot\"

    $cmakeArgs += @(
        "-DGGML_CUDA=ON",
        (New-CMakeCacheArg -Name "CUDAToolkit_ROOT" -Type "PATH" -Value (Convert-ToCMakePath $cudaRoot)),
        (New-CMakeCacheArg -Name "CMAKE_CUDA_COMPILER" -Type "FILEPATH" -Value (Convert-ToCMakePath $nvccPath)),
        (New-CMakeCacheArg -Name "CUDA_CUDART" -Type "FILEPATH" -Value (Convert-ToCMakePath $libDir))
    )
    if ($cudaArchArg) {
        $cmakeArgs += New-CMakeCacheArg -Name "CMAKE_CUDA_ARCHITECTURES" -Value $cudaArchArg
    }
} else {
    $cmakeArgs += "-DGGML_CUDA=OFF"
}

if ($useVulkan -eq "ON") {
    if (-not $vulkanIncludeDir -or -not $vulkanLibPath -or -not $vulkanGlslcPath) {
        throw "Vulkan paths were not initialized even though Vulkan support is enabled."
    }
    $cmakeArgs += @(
        (New-CMakeCacheArg -Name "Vulkan_INCLUDE_DIR" -Type "PATH" -Value (Convert-ToCMakePath $vulkanIncludeDir)),
        (New-CMakeCacheArg -Name "Vulkan_LIBRARY" -Type "FILEPATH" -Value (Convert-ToCMakePath $vulkanLibPath)),
        (New-CMakeCacheArg -Name "Vulkan_GLSLC_EXECUTABLE" -Type "FILEPATH" -Value (Convert-ToCMakePath $vulkanGlslcPath))
    )
}

$cmakeConfigureArgs = @("-Wno-dev", "-S", $llamaDir, "-B", $llamaBuildDir)
if ($useCuda -eq "ON") {
    $cmakeConfigureArgs += @("-G", $visualStudioGenerator, "-A", "x64", "-T", "cuda=$cudaRoot,host=x64")
}
$cmakeConfigureArgs += $cmakeArgs

try {
    $gitSafeDirectoryState = Push-TransientGitSafeDirectory -Directory $llamaDir

    if (Test-Path $llamaBuildDir) {
        Remove-Item -Recurse -Force $llamaBuildDir
    }
    New-Item -ItemType Directory -Force -Path $llamaBuildDir | Out-Null

    & $cmakeExe @cmakeConfigureArgs
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed with exit code $LASTEXITCODE"
    }

    & $cmakeExe --build $llamaBuildDir --config Release -- /m
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed with exit code $LASTEXITCODE"
    }
} finally {
    Pop-TransientGitSafeDirectory -State $gitSafeDirectoryState

    if ($uiDistState -and $uiDistState.CleanupAfterBuild -and (Test-Path $uiDistState.DistDir)) {
        Remove-Item -Recurse -Force $uiDistState.DistDir
    }
}

# --- Clean and repopulate precompiled outputs ---
$variant = "cpu"
$runtimeSubdir = "wocuda"
$legacyVulkanRuntimeDir = $null
if ($useCuda -eq "ON") {
    $variant = "cuda"
    $runtimeSubdir = "wcuda"
} elseif ($useVulkan -eq "ON") {
    if ($enableBlas) {
        $variant = "vulkan-blas"
    } else {
        $variant = "vulkan"
    }
    $runtimeSubdir = "wvulkan"
    $legacyVulkanRuntimeDir = Join-Path $ggmlRuntimeRoot "wvulkan-cpu"
}
$variantRoot = Join-Path $precompiledRootDir $variant
$variantBin = Join-Path $variantRoot "bin"
$variantLib = Join-Path $variantRoot "lib"
$runtimeDir = Join-Path $ggmlRuntimeRoot $runtimeSubdir

if ($legacyVulkanRuntimeDir -and (Test-Path $legacyVulkanRuntimeDir)) {
    Remove-Item -Recurse -Force $legacyVulkanRuntimeDir
}

foreach ($dir in @($variantBin, $variantLib, $runtimeDir)) {
    if (Test-Path $dir) {
        Remove-Item -Recurse -Force $dir
    }
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
}

$releaseBin = Join-Path $llamaBuildDir "bin\Release"
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

if ($enableBlas -and $openBlasDll -and (Test-Path $openBlasDll)) {
    $libOpenBlasName = "libopenblas.dll"
    Copy-Item $openBlasDll -Destination (Join-Path $variantBin $libOpenBlasName) -Force
    Copy-Item $openBlasDll -Destination (Join-Path $runtimeDir $libOpenBlasName) -Force
    foreach ($legacy in @((Join-Path $variantBin "openblas.dll"), (Join-Path $runtimeDir "openblas.dll"))) {
        if (Test-Path $legacy) {
            Remove-Item $legacy -Force
        }
    }

    foreach ($runtimeName in $openBlasCompanionDllNames) {
        $runtimeDllPath = Join-Path $openBlasRoot "bin\$runtimeName"
        if (-not (Test-Path $runtimeDllPath)) {
            continue
        }

        Copy-Item $runtimeDllPath -Destination (Join-Path $variantBin $runtimeName) -Force
        Copy-Item $runtimeDllPath -Destination (Join-Path $runtimeDir $runtimeName) -Force
    }
}
if (Test-Path $curlDll) {
    Copy-Item $curlDll -Destination $variantBin -Force
    Copy-Item $curlDll -Destination $runtimeDir -Force
}

foreach ($runtimeDll in $openSslRuntimeDlls) {
    Copy-Item $runtimeDll.FullName -Destination $variantBin -Force
    Copy-Item $runtimeDll.FullName -Destination $runtimeDir -Force
}

if ($useVulkan -eq "ON" -and $vulkanDllPath -and (Test-Path $vulkanDllPath)) {
    Copy-Item $vulkanDllPath -Destination $variantBin -Force
    Copy-Item $vulkanDllPath -Destination $runtimeDir -Force
}

$importLibNames = @("llama.lib", "ggml.lib", "ggml-base.lib", "ggml-cpu.lib", "mtmd.lib")
$optionalLibs = @("ggml-blas.lib", "ggml-openblas.lib")
if ($useCuda -eq "ON") {
    $importLibNames += "ggml-cuda.lib"
}

foreach ($libName in $importLibNames) {
    $libSource = Get-ChildItem $llamaBuildDir -Filter $libName -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $libSource) {
        throw "Could not locate $libName within the llama.cpp build directory."
    }
    Copy-Item $libSource.FullName -Destination (Join-Path $variantLib $libName) -Force
}
foreach ($libName in $optionalLibs) {
    $libSource = Get-ChildItem $llamaBuildDir -Filter $libName -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($libSource) {
        Copy-Item $libSource.FullName -Destination (Join-Path $variantLib $libName) -Force
    }
}

# --- Copy headers ---
New-Item -ItemType Directory -Force -Path $headersDir | Out-Null
Copy-Item "$llamaDir\include\llama.h" -Destination $headersDir
Copy-Item "$llamaDir\ggml\src\*.h" -Destination $headersDir -ErrorAction SilentlyContinue
Copy-Item "$llamaDir\ggml\include\*.h" -Destination $headersDir -ErrorAction SilentlyContinue
