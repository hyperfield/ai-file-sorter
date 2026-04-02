param(
    [string]$BuildDir = (Join-Path (Split-Path -Parent $PSScriptRoot) "build-windows"),
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$SyncRoot
)

$ErrorActionPreference = "Stop"

function Resolve-TestBinary {
    param(
        [string]$BaseBuildDir,
        [string]$ConfigurationName
    )

    $candidates = @(
        (Join-Path $BaseBuildDir "$ConfigurationName\ai_file_sorter_tests.exe"),
        (Join-Path $BaseBuildDir "ai_file_sorter_tests.exe"),
        (Join-Path (Join-Path (Split-Path -Parent $PSScriptRoot) "build-tests") "ai_file_sorter_tests.exe")
    )

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw "Could not find ai_file_sorter_tests.exe. Build tests first, for example with app\build_windows.ps1 -BuildTests."
}

function Resolve-OneDriveSyncRoot {
    param([string]$ExplicitPath)

    if ($ExplicitPath) {
        return $ExplicitPath
    }
    if ($env:AI_FILE_SORTER_TEST_ONEDRIVE_SYNC_ROOT) {
        return $env:AI_FILE_SORTER_TEST_ONEDRIVE_SYNC_ROOT
    }
    if ($env:OneDrive) {
        return $env:OneDrive
    }
    return $null
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$testsExe = Resolve-TestBinary -BaseBuildDir $BuildDir -ConfigurationName $Configuration
$resolvedSyncRoot = Resolve-OneDriveSyncRoot -ExplicitPath $SyncRoot

if (-not $resolvedSyncRoot) {
    throw "No OneDrive sync root provided. Pass -SyncRoot, or set AI_FILE_SORTER_TEST_ONEDRIVE_SYNC_ROOT, or rely on the OneDrive env var."
}

$resolvedSyncRoot = (Resolve-Path $resolvedSyncRoot).Path
if (-not (Test-Path $resolvedSyncRoot)) {
    throw "OneDrive sync root does not exist: $resolvedSyncRoot"
}

$env:AI_FILE_SORTER_TEST_ONEDRIVE_SYNC_ROOT = $resolvedSyncRoot
$env:QT_QPA_PLATFORM = "offscreen"

$filters = @(
    "OneDriveStorageProvider verifies a real Windows OneDrive sync root via Cloud Files API",
    "External OneDrive connector verifies a real Windows OneDrive sync root via Cloud Files API"
)

Push-Location $repoRoot
try {
    foreach ($filter in $filters) {
        Write-Host "Running: $filter"
        & $testsExe $filter
        if ($LASTEXITCODE -ne 0) {
            throw "Verification failed for test: $filter"
        }
    }
} finally {
    Pop-Location
}

Write-Host ""
Write-Host "Verified OneDrive Windows sync-root detection successfully."
Write-Host "Sync root: $resolvedSyncRoot"
Write-Host "Tests: $testsExe"
