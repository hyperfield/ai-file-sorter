param([Parameter(Mandatory)][string]$Directory, [string]$Python)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\app\scripts\windows_tls_layout.ps1')

function Assert-RejectedLayout {
    param([string]$Path)
    $rejected = $false
    try { Assert-AifsNativeTlsLayout -Directory $Path } catch { $rejected = $true }
    if (-not $rejected) { throw "An unsafe layout was accepted: $Path" }
}

function Assert-UnicodeInstallation {
    param([string]$Source, [string]$Destination)
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    $files = @(Get-ChildItem -LiteralPath $Source -Filter '*.dll' -File)
    $main = Join-Path $Source 'aifilesorter-bin.exe'
    if (-not (Test-Path -LiteralPath $main)) { $main = Join-Path $Source 'aifilesorter.exe' }
    foreach ($file in $files) { Copy-Item -LiteralPath $file.FullName -Destination $Destination }
    Copy-Item -LiteralPath $main -Destination $Destination
    $launcher = Join-Path $Source 'aifilesorter.exe'
    $hasLauncher = (Test-Path -LiteralPath (Join-Path $Source 'aifilesorter-bin.exe')) -and
                   (Test-Path -LiteralPath $launcher)
    if ($hasLauncher) { Copy-Item -LiteralPath $launcher -Destination $Destination }
    $platforms = Join-Path $Source 'platforms'
    if (Test-Path -LiteralPath $platforms) {
        Copy-Item -LiteralPath $platforms -Destination $Destination -Recurse
    }
    $cpu = Join-Path $Source 'lib\ggml\wocuda'
    if (Test-Path -LiteralPath $cpu) {
        $destinationCpu = Join-Path $Destination 'lib\ggml\wocuda'
        New-Item -ItemType Directory -Path $destinationCpu -Force | Out-Null
        Copy-Item -Path (Join-Path $cpu '*') -Destination $destinationCpu -Recurse
    }
    Assert-AifsNativeTlsLayout -Directory $Destination
    if ($hasLauncher -and $Python) {
        $start = New-Object Diagnostics.ProcessStartInfo
        $start.FileName = $Python
        $suite = Join-Path $PSScriptRoot 'integration\test_headless_stdout.py'
        $start.Arguments = '"' + $suite + '" --app "' + (Join-Path $Destination 'aifilesorter.exe') + '" -v'
        $start.UseShellExecute = $false
        $start.CreateNoWindow = $true
        $start.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
        $start.RedirectStandardOutput = $true
        $start.RedirectStandardError = $true
        $process = New-Object Diagnostics.Process
        $process.StartInfo = $start
        try {
            if (-not $process.Start()) { throw 'Could not start Unicode launcher contract checks.' }
            $stdout = $process.StandardOutput.ReadToEndAsync()
            $stderr = $process.StandardError.ReadToEndAsync()
            $timeoutMilliseconds = 45000
            if (-not $process.WaitForExit($timeoutMilliseconds)) {
                # Kill only this test's Python/launcher tree before removing its
                # copied runtime; a blocked GUI child must not outlive the test.
                & taskkill.exe /PID $process.Id /T /F | Out-Null
                $process.WaitForExit()
                Write-Output ($stdout.GetAwaiter().GetResult() + $stderr.GetAwaiter().GetResult())
                throw 'Unicode launcher contract checks timed out.'
            }
            $output = $stdout.GetAwaiter().GetResult() + $stderr.GetAwaiter().GetResult()
            Write-Output $output
            if ($process.ExitCode -ne 0) { throw 'Unicode launcher stdout/runtime propagation failed.' }
        } finally { $process.Dispose() }
    }
}

$temporary = Join-Path ([IO.Path]::GetTempPath()) ('aifs-layout-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $temporary 'certs') -Force | Out-Null
try {
    $bundle = Join-Path $temporary 'certs\cacert.pem'
    $keep = Join-Path $temporary 'certs\unrelated.txt'
    [IO.File]::WriteAllText($bundle, 'obsolete')
    [IO.File]::WriteAllText($keep, 'keep')
    [IO.File]::WriteAllText((Join-Path $temporary 'aifilesorter.exe'), '')
    [IO.File]::WriteAllText((Join-Path $temporary 'libcurl.dll'), '')
    Assert-RejectedLayout -Path $temporary
    Remove-AifsLegacyTlsFiles -Directory $temporary
    if ((Test-Path -LiteralPath $bundle) -or -not (Test-Path -LiteralPath $keep)) {
        throw 'Legacy cleanup removed an unrelated file or retained the bundle.'
    }
    # An upgrade/reused layout is idempotent; nested competing runtimes fail.
    Remove-AifsLegacyTlsFiles -Directory $temporary
    New-Item -ItemType Directory -Path (Join-Path $temporary 'lib\ggml\wocuda') -Force | Out-Null
    [IO.File]::WriteAllText((Join-Path $temporary 'lib\ggml\wocuda\libcurl.dll'), '')
    Assert-RejectedLayout -Path $temporary
    Remove-AifsLegacyTlsFiles -Directory $temporary
    if (Test-Path -LiteralPath (Join-Path $temporary 'lib\ggml\wocuda\libcurl.dll')) {
        throw 'An obsolete backend curl copy survived upgrade cleanup.'
    }
    Assert-AifsNativeTlsLayout -Directory $Directory
    $unicode = Join-Path $temporary ('installed ' + [char]0x00D8 + ' path')
    Assert-UnicodeInstallation -Source $Directory -Destination $unicode
    Write-Output 'Windows native TLS packaging and upgrade checks passed.'
} finally {
    $resolved = (Resolve-Path -LiteralPath $temporary).ProviderPath
    if ($resolved -ieq [IO.Path]::GetFullPath($temporary)) {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
