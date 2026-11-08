# Shared native-Windows trust and layout gates. Dot-sourcing never executes an app.

function Remove-AifsLegacyTlsFiles {
    <# .SYNOPSIS Removes the retired CA file and known obsolete backend curl copies. #>
    param([Parameter(Mandatory)][string]$Directory)
    $root = (Resolve-Path -LiteralPath $Directory -ErrorAction Stop).ProviderPath
    $legacyFiles = @('certs\cacert.pem', 'lib\ggml\wocuda\libcurl.dll',
                     'lib\ggml\wcuda\libcurl.dll', 'lib\ggml\wvulkan\libcurl.dll')
    foreach ($relative in $legacyFiles) {
        $path = $root
        foreach ($part in $relative.Split('\')) {
            $path = Join-Path $path $part
            if ((Test-Path -LiteralPath $path) -and
                ((Get-Item -LiteralPath $path -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
                throw "Refusing legacy TLS cleanup through a reparse point: $path"
            }
        }
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            Remove-Item -LiteralPath $path -Force -ErrorAction Stop
        }
    }
}

function Assert-AifsNativeTlsLayout {
    <# .SYNOPSIS Runs the production TLS self-test and checks the loaded adjacent curl DLL. #>
    param([Parameter(Mandatory)][string]$Directory)
    $root = (Resolve-Path -LiteralPath $Directory -ErrorAction Stop).ProviderPath
    $dll = Join-Path $root 'libcurl.dll'
    $main = Join-Path $root 'aifilesorter-bin.exe'
    if (-not (Test-Path -LiteralPath $main -PathType Leaf)) { $main = Join-Path $root 'aifilesorter.exe' }
    if (-not (Test-Path -LiteralPath $main -PathType Leaf) -or -not (Test-Path -LiteralPath $dll -PathType Leaf)) {
        throw "The Windows TLS gate requires the production executable and adjacent libcurl.dll: $root"
    }
    $bundles = @(Get-ChildItem -LiteralPath $root -Filter 'cacert.pem' -File -Recurse)
    $duplicates = @(Get-ChildItem -LiteralPath $root -Filter 'libcurl.dll' -File -Recurse |
        Where-Object { $_.FullName -ine $dll })
    if ($bundles.Count -or $duplicates.Count) {
        throw "The Windows layout contains obsolete CA bundles or competing curl DLLs: $root"
    }
    $temporary = Join-Path ([IO.Path]::GetTempPath()) ('aifs-tls-gate-' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $temporary -ErrorAction Stop | Out-Null
    $priorConfig = $env:AI_FILE_SORTER_CONFIG_DIR
    $process = $null
    try {
        $env:AI_FILE_SORTER_CONFIG_DIR = $temporary
        $start = New-Object Diagnostics.ProcessStartInfo
        $start.FileName = $main
        $start.Arguments = '--cuda=off --vulkan=off --self-test=tls'
        $start.WorkingDirectory = $temporary
        $start.UseShellExecute = $false
        $start.CreateNoWindow = $true
        $start.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
        $start.RedirectStandardOutput = $true
        $start.RedirectStandardError = $true
        $start.StandardOutputEncoding = New-Object Text.UTF8Encoding
        $start.StandardErrorEncoding = New-Object Text.UTF8Encoding
        $process = New-Object Diagnostics.Process
        $process.StartInfo = $start
        if (-not $process.Start()) { throw 'Could not start the production TLS self-test.' }
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $timeoutMilliseconds = 30000
        if (-not $process.WaitForExit($timeoutMilliseconds)) {
            $process.Kill()
            throw 'The production TLS self-test timed out.'
        }
        $output = $stdout.GetAwaiter().GetResult() + $stderr.GetAwaiter().GetResult()
        if ($process.ExitCode -ne 0 -or $output -notmatch '\[PASS\] platform TLS trust' -or
            $output -notmatch 'Schannel' -or $output -notmatch ('; DLL: ' + [regex]::Escape($dll) + '(\r?\n|$)')) {
            throw "The production TLS runtime failed native Schannel/path validation: $output"
        }
        Write-Output $output.Trim()
        Write-Output ("Verified Windows curl SHA256: {0}" -f (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash)
    } finally {
        if ($process) { $process.Dispose() }
        $env:AI_FILE_SORTER_CONFIG_DIR = $priorConfig
        # This GUID directory was created immediately above under the system temp
        # directory; its resolved absolute path is the only recursive cleanup target.
        $resolved = (Resolve-Path -LiteralPath $temporary).ProviderPath
        $expected = [IO.Path]::GetFullPath($temporary)
        if ($resolved -ieq $expected) { Remove-Item -LiteralPath $resolved -Recurse -Force }
    }
}
