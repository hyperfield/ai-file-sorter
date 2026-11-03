# Shared by the llama.cpp runtime build and application packaging. Only CUDA
# redistributables are copied; the NVIDIA display driver remains system-owned.

function Get-WindowsCudaToolkitRoots {
    $roots = @($env:CUDA_PATH)
    $roots += @(Get-ChildItem Env:CUDA_PATH_V* -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Value)
    $toolkitRoot = Join-Path $env:ProgramFiles 'NVIDIA GPU Computing Toolkit\CUDA'
    if (Test-Path -LiteralPath $toolkitRoot) {
        $roots += @(Get-ChildItem -LiteralPath $toolkitRoot -Directory |
            Where-Object { $_.Name -match '^v\d+(\.\d+)*$' } |
            Sort-Object { [version]$_.Name.Substring(1) } -Descending |
            Select-Object -ExpandProperty FullName)
    }
    return @($roots | Where-Object { $_ } | Select-Object -Unique)
}

function Get-WindowsCudaDllImports {
    param([Parameter(Mandatory)][string]$Path)

    $imports = & dumpbin.exe /nologo /dependents $Path 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "Could not inspect CUDA DLL dependencies for '$Path': $imports"
    }
    return @($imports | ForEach-Object {
        if ($_ -match '^\s*((?:cublas(?:Lt)?|cudart)64_\d+\.dll)\s*$') {
            $Matches[1]
        }
    } | Select-Object -Unique)
}

function Get-WindowsCudaRuntimeSet {
    param([string]$Directory, [string[]]$Imports)

    $required = @($Imports)
    foreach ($name in $Imports) {
        if ($name -match '^cublas64_(\d+)\.dll$') {
            # cuBLAS can load cuBLASLt dynamically, so imports alone are insufficient.
            $required += "cublasLt64_$($Matches[1]).dll"
            $major = $Matches[1]
        }
    }
    if (-not ($required -match '^cudart64_')) {
        # ggml may link cudart statically; the app also needs it for device/memory probes.
        $runtimes = @(Get-ChildItem -LiteralPath $Directory -Filter 'cudart64_*.dll' -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match "^cudart64_${major}(\d*)\.dll$" })
        if ($runtimes.Count -ne 1) { return $null }
        $required += $runtimes[0].Name
    }
    $required = @($required | Select-Object -Unique)
    foreach ($name in $required) {
        if (-not (Test-Path -LiteralPath (Join-Path $Directory $name) -PathType Leaf)) { return $null }
    }
    return ,$required
}

function Find-WindowsCudaLicense {
    param([string]$Directory)
    $parent = Split-Path $Directory -Parent
    foreach ($path in @((Join-Path $Directory 'CUDA-EULA.txt'), (Join-Path $parent 'EULA.txt'),
                        (Join-Path (Split-Path $parent -Parent) 'EULA.txt'))) {
        if (Test-Path -LiteralPath $path -PathType Leaf) { return $path }
    }
    return $null
}

function Copy-WindowsCudaRuntimeFiles {
    param([string]$Source, [string]$Destination, [string[]]$Files, [string]$License)
    foreach ($name in $Files) {
        $from = Join-Path $Source $name
        $to = Join-Path $Destination $name
        if ([IO.Path]::GetFullPath($from) -ne [IO.Path]::GetFullPath($to)) {
            Copy-Item -LiteralPath $from -Destination $to -Force
        }
    }
    $licenseTarget = Join-Path $Destination 'CUDA-EULA.txt'
    if ([IO.Path]::GetFullPath($License) -ne [IO.Path]::GetFullPath($licenseTarget)) {
        Copy-Item -LiteralPath $License -Destination $licenseTarget -Force
    }
}

function Copy-WindowsCudaRuntime {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$RuntimeDirectory,
        [string[]]$SourceDirectories = @(),
        [string[]]$ToolkitRoots = (Get-WindowsCudaToolkitRoots),
        [scriptblock]$DependencyReader = { param($Path) Get-WindowsCudaDllImports -Path $Path }
    )

    $backend = Join-Path $RuntimeDirectory 'ggml-cuda.dll'
    if (-not (Test-Path -LiteralPath $backend -PathType Leaf)) {
        throw "CUDA backend is missing: $backend"
    }
    $imports = @(& $DependencyReader $backend)
    if (-not ($imports -match '^cublas64_\d+\.dll$')) {
        throw "No supported cuBLAS dependency found in '$backend'."
    }

    $sources = @($RuntimeDirectory) + $SourceDirectories
    foreach ($root in $ToolkitRoots) {
        $sources += @(Join-Path $root 'bin\x64'; Join-Path $root 'bin')
    }
    foreach ($source in $sources | Select-Object -Unique) {
        $files = Get-WindowsCudaRuntimeSet -Directory $source -Imports $imports
        if (-not $files) { continue }
        $license = Find-WindowsCudaLicense -Directory $source
        if (-not $license) { continue }
        Copy-WindowsCudaRuntimeFiles -Source $source -Destination $RuntimeDirectory -Files $files -License $license
        Write-Host "Staged CUDA redistributables from '$source': $($files -join ', ')"
        return
    }
    throw "Incomplete CUDA payload in '$RuntimeDirectory'. Stage matching cudart/cuBLAS/cuBLASLt DLLs and CUDA-EULA.txt, or set CUDA_PATH to a toolkit providing $($imports -join ', ')."
}
