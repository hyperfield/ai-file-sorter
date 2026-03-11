param(
    [Alias("time-period")]
    [string]$TimePeriod,
    [Alias("output-dir")]
    [string]$OutputDir = [System.Environment]::GetFolderPath("Desktop"),
    [Alias("keep-raw")]
    [switch]$KeepRaw,
    [Alias("open-output")]
    [switch]$OpenOutput,
    [Alias("h", "help")]
    [switch]$ShowHelp
)

$ErrorActionPreference = "Stop"

function Show-Usage {
    @"
Collect, redact, and zip AI File Sorter diagnostics on Windows.

Usage:
  .\collect_windows_diagnostics.ps1 [options]

Options:
  -TimePeriod <duration>  Collect logs from the last duration (e.g. 30m, 1h, 2h30m, 1d)
  -OutputDir <path>       Output directory for collected artifacts (default: Desktop)
  -KeepRaw                Keep unredacted raw logs on disk (default: remove raw logs)
  -OpenOutput             Reveal resulting zip in Explorer
  -ShowHelp               Show this help

Default behavior:
  If -TimePeriod is not supplied, the script attempts "latest run" mode:
  it finds the newest app log mtime and collects logs around that window.

Examples:
  .\collect_windows_diagnostics.ps1
  .\collect_windows_diagnostics.ps1 -TimePeriod 1h
  .\collect_windows_diagnostics.ps1 -TimePeriod 90m -OutputDir "$env:USERPROFILE\Desktop"
"@
}

function Convert-TimePeriodToTimeSpan {
    param([Parameter(Mandatory = $true)][string]$InputValue)

    $value = $InputValue.Trim().ToLowerInvariant() -replace "\s+", ""
    if ([string]::IsNullOrWhiteSpace($value)) {
        throw "Empty duration."
    }

    if ($value -match '^\d+$') {
        return [TimeSpan]::FromSeconds([int64]$value)
    }

    $rest = $value
    $totalSeconds = [int64]0

    while ($rest.Length -gt 0) {
        $m = [regex]::Match($rest, '^([0-9]+)([smhd])(.*)$')
        if (-not $m.Success) {
            throw "Invalid duration format: '$InputValue' (examples: 30m, 1h, 2h30m, 1d)."
        }

        $number = [int64]$m.Groups[1].Value
        $unit = $m.Groups[2].Value
        $rest = $m.Groups[3].Value

        switch ($unit) {
            "s" { $totalSeconds += $number }
            "m" { $totalSeconds += $number * 60 }
            "h" { $totalSeconds += $number * 3600 }
            "d" { $totalSeconds += $number * 86400 }
            default { throw "Unsupported duration unit in '$InputValue'." }
        }
    }

    if ($totalSeconds -le 0) {
        throw "Duration must be greater than zero."
    }

    return [TimeSpan]::FromSeconds($totalSeconds)
}

function New-DirectoryIfMissing {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Copy-RecentAppLogs {
    param(
        [Parameter(Mandatory = $true)][string]$SourceDir,
        [Parameter(Mandatory = $true)][string]$DestinationDir,
        [Parameter(Mandatory = $true)][datetime]$Since
    )

    if (-not (Test-Path -LiteralPath $SourceDir)) {
        return 0
    }

    New-DirectoryIfMissing -Path $DestinationDir
    $copied = 0
    $patterns = @("core.log*", "db.log*", "ui.log*")
    foreach ($pattern in $patterns) {
        $files = Get-ChildItem -Path $SourceDir -Filter $pattern -File -ErrorAction SilentlyContinue
        foreach ($file in $files) {
            if ($file.LastWriteTime -ge $Since) {
                Copy-Item -LiteralPath $file.FullName -Destination $DestinationDir -Force
                $copied++
            }
        }
    }

    return $copied
}

function Get-LatestAppLogTime {
    param([Parameter(Mandatory = $true)][string[]]$LogDirs)

    $latest = [datetime]::MinValue
    foreach ($dir in $LogDirs) {
        if (-not (Test-Path -LiteralPath $dir)) { continue }
        $patterns = @("core.log*", "db.log*", "ui.log*")
        foreach ($pattern in $patterns) {
            $files = Get-ChildItem -Path $dir -Filter $pattern -File -ErrorAction SilentlyContinue
            foreach ($file in $files) {
                if ($file.LastWriteTime -gt $latest) {
                    $latest = $file.LastWriteTime
                }
            }
        }
    }
    return $latest
}

function Is-TextLikeFile {
    param([Parameter(Mandatory = $true)][string]$Path)
    $ext = [IO.Path]::GetExtension($Path).ToLowerInvariant()
    $textExtensions = @(
        ".log", ".txt", ".json", ".wer", ".xml",
        ".ini", ".cfg", ".conf", ".out", ".err", ".csv"
    )
    return $textExtensions -contains $ext
}

function Redact-TextContent {
    param([Parameter(Mandatory = $true)][string]$Content)

    $result = $Content

    if ($env:USERPROFILE) {
        $result = [regex]::Replace($result, [regex]::Escape($env:USERPROFILE), "<HOME>", "IgnoreCase")
    }
    if ($env:HOMEDRIVE -and $env:HOMEPATH) {
        $homePath = "$($env:HOMEDRIVE)$($env:HOMEPATH)"
        $result = [regex]::Replace($result, [regex]::Escape($homePath), "<HOME>", "IgnoreCase")
    }

    $result = [regex]::Replace($result, '(?i)\b([A-Z]:\\Users\\)[^\\\s]+', '$1<user>')
    $result = [regex]::Replace($result, '(?i)\b([A-Z]:\\Documents and Settings\\)[^\\\s]+', '$1<user>')

    $result = [regex]::Replace($result, '(?i)(Authorization:\s*Bearer\s+)[^\s"''`]+', '$1<REDACTED>')
    $result = [regex]::Replace($result, '(?i)([?&](?:key|api_key|apikey|token|auth|authorization)=)[^&\s"''`]+', '$1<REDACTED>')
    $result = [regex]::Replace($result, '\bsk-(?:proj-)?[A-Za-z0-9_-]{8,}\b', 'sk-<REDACTED>')
    $result = [regex]::Replace($result, '\bAIza[0-9A-Za-z_-]{20,}\b', 'AIza<REDACTED>')

    return $result
}

if ($ShowHelp) {
    Show-Usage
    exit 0
}

if ([System.Environment]::OSVersion.Platform -ne [System.PlatformID]::Win32NT) {
    throw "This script is for Windows only."
}

if (-not (Get-Command Compress-Archive -ErrorAction SilentlyContinue)) {
    throw "Compress-Archive cmdlet is unavailable."
}

if ([string]::IsNullOrWhiteSpace($OutputDir) -or -not (Test-Path -LiteralPath $OutputDir)) {
    $OutputDir = $env:USERPROFILE
}
New-DirectoryIfMissing -Path $OutputDir
$OutputDir = (Resolve-Path -LiteralPath $OutputDir).Path

$logDirs = @()
if ($env:APPDATA) {
    $logDirs += (Join-Path $env:APPDATA "AIFileSorter\logs")
}

$now = Get-Date
$since = $null
$windowNote = ""

if (-not [string]::IsNullOrWhiteSpace($TimePeriod)) {
    $span = Convert-TimePeriodToTimeSpan -InputValue $TimePeriod
    $since = $now - $span
    $windowNote = "time-period mode (-TimePeriod $TimePeriod)"
} else {
    $latest = Get-LatestAppLogTime -LogDirs $logDirs
    if ($latest -gt [datetime]::MinValue) {
        $since = $latest.AddMinutes(-5)
        if ($since -lt [datetime]::UnixEpoch) {
            $since = [datetime]::UnixEpoch
        }
        $windowNote = "latest-run mode (newest app log mtime: $($latest.ToString("yyyy-MM-dd HH:mm:ss")))"
    } else {
        $since = $now.AddHours(-1)
        $windowNote = "fallback mode (no app logs found, using last 1h)"
        Write-Warning "No app logs found. Falling back to last 1h."
    }
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$baseName = "aifs-windows-diagnostics-$timestamp"
$workDir = Join-Path $OutputDir $baseName
$rawDir = Join-Path $workDir "raw"
$redactedDir = Join-Path $workDir "redacted"
$zipPath = Join-Path $OutputDir "$baseName-redacted.zip"

New-DirectoryIfMissing -Path $rawDir
New-DirectoryIfMissing -Path $redactedDir

$appLogsCopied = 0
if ($env:APPDATA) {
    $appLogsCopied += Copy-RecentAppLogs -SourceDir (Join-Path $env:APPDATA "AIFileSorter\logs") -DestinationDir (Join-Path $rawDir "appdata-logs") -Since $since
}

if ($appLogsCopied -eq 0) {
    Write-Warning "No recent app log files matched the selected window."
}

$crashRoot = Join-Path $rawDir "crash"
New-DirectoryIfMissing -Path $crashRoot
$crashCount = 0

if ($env:LOCALAPPDATA) {
    $crashDumpDir = Join-Path $env:LOCALAPPDATA "CrashDumps"
    if (Test-Path -LiteralPath $crashDumpDir) {
        $dumpDest = Join-Path $crashRoot "CrashDumps"
        New-DirectoryIfMissing -Path $dumpDest
        $dumps = Get-ChildItem -Path $crashDumpDir -File -ErrorAction SilentlyContinue |
            Where-Object {
                ($_.Name -match '(?i)aifilesorter') -and ($_.LastWriteTime -ge $since)
            }
        foreach ($dump in $dumps) {
            Copy-Item -LiteralPath $dump.FullName -Destination $dumpDest -Force
            $crashCount++
        }
    }

    $werBase = Join-Path $env:LOCALAPPDATA "Microsoft\Windows\WER"
    foreach ($sub in @("ReportArchive", "ReportQueue")) {
        $dir = Join-Path $werBase $sub
        if (-not (Test-Path -LiteralPath $dir)) { continue }

        $dest = Join-Path $crashRoot "WER\$sub"
        New-DirectoryIfMissing -Path $dest

        $reportDirs = Get-ChildItem -Path $dir -Directory -ErrorAction SilentlyContinue |
            Where-Object {
                ($_.Name -match '(?i)aifilesorter') -and ($_.LastWriteTime -ge $since)
            }
        foreach ($reportDir in $reportDirs) {
            Copy-Item -LiteralPath $reportDir.FullName -Destination $dest -Recurse -Force
            $crashCount++
        }
    }
}

$eventsDir = Join-Path $rawDir "events"
New-DirectoryIfMissing -Path $eventsDir

try {
    $appEvents = Get-WinEvent -FilterHashtable @{ LogName = "Application"; StartTime = $since } -ErrorAction Stop |
        Where-Object { $_.Message -match '(?i)aifilesorter|AIFileSorter' }
    if ($appEvents) {
        $appEvents |
            Select-Object TimeCreated, Id, LevelDisplayName, ProviderName, Message |
            Format-List | Out-File -FilePath (Join-Path $eventsDir "application_events.txt") -Encoding utf8
    } else {
        "No matching Application events." | Out-File -FilePath (Join-Path $eventsDir "application_events.txt") -Encoding utf8
    }
} catch {
    "Failed to collect Application events: $($_.Exception.Message)" |
        Out-File -FilePath (Join-Path $eventsDir "application_events.txt") -Encoding utf8
}

try {
    $sysEvents = Get-WinEvent -FilterHashtable @{ LogName = "System"; StartTime = $since } -ErrorAction Stop |
        Where-Object { $_.Message -match '(?i)aifilesorter|AIFileSorter' }
    if ($sysEvents) {
        $sysEvents |
            Select-Object TimeCreated, Id, LevelDisplayName, ProviderName, Message |
            Format-List | Out-File -FilePath (Join-Path $eventsDir "system_events.txt") -Encoding utf8
    } else {
        "No matching System events." | Out-File -FilePath (Join-Path $eventsDir "system_events.txt") -Encoding utf8
    }
} catch {
    "Failed to collect System events: $($_.Exception.Message)" |
        Out-File -FilePath (Join-Path $eventsDir "system_events.txt") -Encoding utf8
}

$systemInfoFile = Join-Path $rawDir "system_info.txt"
@(
    "Collected at: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz')"
    "Window note: $windowNote"
    "Window start: $($since.ToString('yyyy-MM-dd HH:mm:ss'))"
    "App logs copied: $appLogsCopied"
    "Crash artifacts copied: $crashCount"
    ""
    "== OS =="
    (Get-CimInstance Win32_OperatingSystem | Select-Object -ExpandProperty Caption)
    (Get-CimInstance Win32_OperatingSystem | Select-Object -ExpandProperty Version)
    ""
    "== CPU =="
    (Get-CimInstance Win32_Processor | Select-Object -First 1 -ExpandProperty Name)
    ""
    "== Computer =="
    $env:COMPUTERNAME
) | Out-File -FilePath $systemInfoFile -Encoding utf8

$rawFiles = Get-ChildItem -Path $rawDir -Recurse -File -ErrorAction SilentlyContinue
foreach ($srcFile in $rawFiles) {
    $relative = $srcFile.FullName.Substring($rawDir.Length).TrimStart('\')
    $dst = Join-Path $redactedDir $relative
    $dstParent = Split-Path -Parent $dst
    New-DirectoryIfMissing -Path $dstParent

    if (Is-TextLikeFile -Path $srcFile.FullName) {
        try {
            $content = Get-Content -LiteralPath $srcFile.FullName -Raw -ErrorAction Stop
            $redacted = Redact-TextContent -Content $content
            Set-Content -LiteralPath $dst -Value $redacted -Encoding utf8
        } catch {
            Copy-Item -LiteralPath $srcFile.FullName -Destination $dst -Force
        }
    } else {
        Copy-Item -LiteralPath $srcFile.FullName -Destination $dst -Force
    }
}

if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
Compress-Archive -Path (Join-Path $workDir "redacted") -DestinationPath $zipPath -Force

if (-not $KeepRaw) {
    Remove-Item -LiteralPath $rawDir -Recurse -Force
}

Write-Output "Diagnostics bundle ready:"
Write-Output "  $zipPath"
Write-Output ""
Write-Output "Collected with: $windowNote"
Write-Output "If needed, inspect redacted files at:"
Write-Output "  $redactedDir"

if ($OpenOutput) {
    Start-Process explorer.exe "/select,`"$zipPath`"" | Out-Null
}
