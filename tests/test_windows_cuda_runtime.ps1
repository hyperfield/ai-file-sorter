$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../app/scripts/stage_windows_cuda_runtime.ps1')

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

function Assert-Throws {
    param([scriptblock]$Action, [string]$Pattern)
    try { & $Action } catch {
        Assert-True ($_.Exception.Message -like $Pattern) "Unexpected error: $_"
        return
    }
    throw "Expected failure matching '$Pattern'."
}

function New-BackendFixture {
    param([string]$Name, [string]$Imports = 'cublas64_13.dll')
    $directory = Join-Path $script:fixtureRoot $Name
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $directory 'ggml-cuda.dll') -Value $Imports
    return $directory
}

function New-ToolkitFixture {
    param([string]$Name, [string]$Layout = 'bin/x64', [string[]]$Files)
    $root = Join-Path $script:fixtureRoot $Name
    $bin = Join-Path $root $Layout
    New-Item -ItemType Directory -Path $bin -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $root 'EULA.txt') -Value 'fixture license'
    foreach ($fileName in $Files) { Set-Content -LiteralPath (Join-Path $bin $fileName) -Value $Name }
    return $root
}

$fixtureRoot = Join-Path ([IO.Path]::GetTempPath()) ('aifs-cuda-runtime-' + [guid]::NewGuid().ToString('N'))
$reader = { param($Path) Get-Content -LiteralPath $Path }
$cuda13Files = @('cudart64_13.dll', 'cublas64_13.dll', 'cublasLt64_13.dll')
try {
    $cuda13 = New-ToolkitFixture 'CUDA 13' -Files $cuda13Files
    $cuda12 = New-ToolkitFixture 'CUDA 12' -Layout 'bin' -Files @('cudart64_12.dll', 'cublas64_12.dll', 'cublasLt64_12.dll')
    $output = New-BackendFixture 'output'
    Copy-WindowsCudaRuntime -RuntimeDirectory $output -ToolkitRoots @($cuda12, $cuda13) -DependencyReader $reader
    foreach ($name in $cuda13Files + @('CUDA-EULA.txt')) {
        Assert-True (Test-Path -LiteralPath (Join-Path $output $name)) "Missing packaged dependency: $name"
    }
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $output 'cublas64_12.dll'))) 'Mixed CUDA major versions.'

    # A staged runtime is reusable on a build host without an installed toolkit.
    $repacked = New-BackendFixture 'repacked'
    Copy-WindowsCudaRuntime -RuntimeDirectory $repacked -SourceDirectories @($output) -ToolkitRoots @() -DependencyReader $reader
    Copy-WindowsCudaRuntime -RuntimeDirectory $repacked -ToolkitRoots @() -DependencyReader $reader
    Assert-True ((Get-Content -LiteralPath (Join-Path $repacked 'cublasLt64_13.dll')) -eq 'CUDA 13') 'Wrong staged library.'

    # Static cudart still needs its matching DLL for the app's memory/device probe.
    $legacy = New-BackendFixture 'legacy' -Imports "cublas64_12.dll`ncudart64_12.dll"
    Copy-WindowsCudaRuntime -RuntimeDirectory $legacy -ToolkitRoots @($cuda12) -DependencyReader $reader
    Assert-True (Test-Path -LiteralPath (Join-Path $legacy 'cudart64_12.dll')) 'Legacy bin layout not supported.'

    $mismatch = New-BackendFixture 'mismatch'
    Assert-Throws { Copy-WindowsCudaRuntime -RuntimeDirectory $mismatch -ToolkitRoots @($cuda12) -DependencyReader $reader } '*Incomplete CUDA payload*'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $mismatch 'cudart64_12.dll'))) 'Partial mismatched payload copied.'

    $partial = New-ToolkitFixture 'partial' -Files @('cudart64_13.dll', 'cublas64_13.dll')
    Assert-Throws { Copy-WindowsCudaRuntime -RuntimeDirectory $mismatch -ToolkitRoots @($partial) -DependencyReader $reader } '*Incomplete CUDA payload*'

    Remove-Item -LiteralPath (Join-Path $partial 'EULA.txt')
    Set-Content -LiteralPath (Join-Path $partial 'bin/x64/cublasLt64_13.dll') -Value 'fixture'
    Assert-Throws { Copy-WindowsCudaRuntime -RuntimeDirectory $mismatch -ToolkitRoots @($partial) -DependencyReader $reader } '*Incomplete CUDA payload*'

    Assert-Throws { Copy-WindowsCudaRuntime -RuntimeDirectory $fixtureRoot -ToolkitRoots @() -DependencyReader $reader } '*CUDA backend is missing*'
    Write-Output 'Windows CUDA runtime staging tests passed.'
} finally {
    $resolvedRoot = [IO.Path]::GetFullPath($fixtureRoot)
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    if (-not $resolvedRoot.StartsWith($tempRoot) -or (Split-Path $resolvedRoot -Leaf) -notlike 'aifs-cuda-runtime-*') {
        throw 'Refusing to remove an unexpected fixture directory.'
    }
    if (Test-Path -LiteralPath $resolvedRoot) { Remove-Item -LiteralPath $resolvedRoot -Recurse -Force }
}
