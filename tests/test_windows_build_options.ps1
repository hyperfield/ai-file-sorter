$ErrorActionPreference = 'Stop'
$buildScript = Join-Path $PSScriptRoot '../app/build_windows.ps1'
$tokens = $null
$parseErrors = $null
$tree = [Management.Automation.Language.Parser]::ParseFile($buildScript, [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count) { throw 'Windows build script could not be parsed.' }
$definition = $tree.Find({ param($node)
    $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Get-ConfigureArguments'
}, $true)
if (-not $definition) { throw 'Windows build configure function is missing.' }
Invoke-Expression $definition.Extent.Text

$appDir = Join-Path ([IO.Path]::GetTempPath()) 'aifs-build-options'
$sharedVcpkgInstalledDir = Join-Path $appDir 'dependencies'
$Configuration = 'Release'
$FolderStructurePluginPublicKeys = ''
$StoragePluginPublicKeys = ''
$PluginEntitlementPublicKeys = ''
$EnablePluginEntitlementDevBypass = $false
$variant = [pscustomobject]@{ Name='Standard'; BuildDir=(Join-Path $appDir 'build'); UpdateMode='AUTO_INSTALL'; PackageKind='STANDARD' }
$cases = @(
    @{ Tests=$true; Live=$true },
    @{ Tests=$true; Live=$false },
    @{ Tests=$false; Live=$false }
)
foreach ($Generator in @('Ninja', 'Visual Studio 18 2026')) {
    foreach ($case in $cases) {
        $enableTests = [bool]$case.Tests
        $enableLive = [bool]$case.Live
        $arguments = Get-ConfigureArguments -Variant $variant -ToolchainFile 'toolchain.cmake' `
            -EnableTests:$enableTests -EnableLiveLlmTests:$enableLive -CMakeMajor 4 -CMakeMinor 3
        $tests = if ($case.Tests) { 'ON' } else { 'OFF' }
        $live = if ($case.Live) { 'ON' } else { 'OFF' }
        if ($arguments -notcontains "-DAI_FILE_SORTER_BUILD_TESTS=$tests") {
            throw "Missing explicit test state $tests for $Generator. Cached test settings could survive."
        }
        if ($arguments -notcontains "-DAI_FILE_SORTER_ENABLE_LIVE_LLM_TESTS=$live") {
            throw "Missing explicit live-test state $live for $Generator."
        }
    }
}
Write-Output 'Windows build options: explicit test ON/OFF states passed for Ninja and Visual Studio.'
