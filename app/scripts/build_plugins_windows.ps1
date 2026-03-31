param(
    [string[]]$Plugins,
    [switch]$List,
    [switch]$Interactive,
    [string]$BuildDir,
    [string]$Configuration = "Release",
    [string]$Generator
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$repoRoot = (Resolve-Path (Join-Path $scriptDir "..\..")).Path
$manifestPath = Join-Path $scriptDir "plugin_build_targets.tsv"
if (-not $BuildDir) {
    $BuildDir = Join-Path $repoRoot "build-plugins-windows"
}

function Get-PluginDefinitions {
    param([string]$Platform)

    if (-not (Test-Path $manifestPath)) {
        throw "Missing plugin manifest: $manifestPath"
    }

    $definitions = @()
    foreach ($line in Get-Content -Path $manifestPath) {
        if ([string]::IsNullOrWhiteSpace($line) -or $line.StartsWith("#")) {
            continue
        }

        $parts = $line -split "`t"
        if ($parts.Count -lt 5) {
            continue
        }

        $platforms = $parts[4].Split(",") | ForEach-Object { $_.Trim().ToLowerInvariant() }
        if ($platforms -notcontains $Platform -and
            $platforms -notcontains "all" -and
            $platforms -notcontains "any") {
            continue
        }

        $definitions += [pscustomobject]@{
            Id         = $parts[0]
            Name       = $parts[1]
            Target     = $parts[2]
            OutputStem = $parts[3]
            Platforms  = $parts[4]
        }
    }

    if (-not $definitions) {
        throw "No plugin build targets are registered for $Platform."
    }

    return $definitions
}

function Show-PluginDefinitions {
    param([object[]]$Definitions)

    Write-Output "Available plugin build targets for windows:"
    foreach ($plugin in $Definitions) {
        Write-Output ("  {0,-28} {1} (CMake target: {2})" -f $plugin.Id, $plugin.Name, $plugin.Target)
    }
}

function Select-PluginDefinitions {
    param([object[]]$Definitions)

    if (Get-Command Out-GridView -ErrorAction SilentlyContinue) {
        $selection = $Definitions |
            Select-Object @{Name = "Plugin ID"; Expression = { $_.Id } },
                          @{Name = "Name"; Expression = { $_.Name } },
                          @{Name = "CMake Target"; Expression = { $_.Target } } |
            Out-GridView -Title "Select AI File Sorter plugins to build (Cancel = all)" -PassThru

        if ($selection) {
            $selectedIds = @($selection | ForEach-Object { $_.'Plugin ID' })
            return $Definitions | Where-Object { $selectedIds -contains $_.Id }
        }
    }

    Show-PluginDefinitions -Definitions $Definitions
    $answer = Read-Host "Enter comma-separated plugin IDs to build [all]"
    if ([string]::IsNullOrWhiteSpace($answer)) {
        return $Definitions
    }

    $requestedIds = $answer.Split(",") | ForEach-Object { $_.Trim() } | Where-Object { $_ }
    $selected = @()
    foreach ($pluginId in $requestedIds) {
        $match = $Definitions | Where-Object { $_.Id -eq $pluginId } | Select-Object -First 1
        if (-not $match) {
            throw "Unknown plugin id for windows: $pluginId"
        }
        $selected += $match
    }

    return $selected
}

function Resolve-CMake {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }
    throw "cmake executable not found in PATH."
}

$definitions = Get-PluginDefinitions -Platform "windows"

if ($List) {
    Show-PluginDefinitions -Definitions $definitions
    exit 0
}

$selectedDefinitions = @()
if ($Plugins) {
    $requested = @()
    foreach ($entry in $Plugins) {
        $requested += ($entry -split "," | ForEach-Object { $_.Trim() } | Where-Object { $_ })
    }

    foreach ($pluginId in $requested) {
        $match = $definitions | Where-Object { $_.Id -eq $pluginId } | Select-Object -First 1
        if (-not $match) {
            throw "Unknown plugin id for windows: $pluginId"
        }
        $selectedDefinitions += $match
    }
} elseif ($Interactive) {
    $selectedDefinitions = Select-PluginDefinitions -Definitions $definitions
} else {
    $selectedDefinitions = $definitions
}

if (-not $selectedDefinitions) {
    throw "No plugins selected."
}

$cmakeExe = Resolve-CMake

$configureArgs = @(
    "-S", (Join-Path $repoRoot "app"),
    "-B", $BuildDir,
    "-DAI_FILE_SORTER_BUILD_TESTS=ON"
)
if ($Generator) {
    $configureArgs += @("-G", $Generator)
}

Write-Output "[INFO] Configuring plugin build directory at $BuildDir"
& $cmakeExe @configureArgs
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed with exit code $LASTEXITCODE"
}

$targets = @($selectedDefinitions | ForEach-Object { $_.Target })
Write-Output "[INFO] Building plugin targets: $($targets -join ', ')"
& $cmakeExe --build $BuildDir --config $Configuration --target @targets -- /m
if ($LASTEXITCODE -ne 0) {
    throw "CMake build failed with exit code $LASTEXITCODE"
}

Write-Output "[INFO] Requested plugin IDs:"
foreach ($plugin in $selectedDefinitions) {
    Write-Output "  - $($plugin.Id)"
}

Write-Output "[INFO] Built output locations:"
foreach ($plugin in $selectedDefinitions) {
    $matches = Get-ChildItem -Path $BuildDir -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Name -eq $plugin.OutputStem -or
            $_.Name -eq "$($plugin.OutputStem).exe" -or
            $_.BaseName -eq $plugin.Target
        } |
        Select-Object -ExpandProperty FullName -Unique

    if ($matches) {
        foreach ($match in $matches) {
            Write-Output "  - $match"
        }
    } else {
        Write-Output "  - $($plugin.OutputStem) (target $($plugin.Target))"
    }
}
