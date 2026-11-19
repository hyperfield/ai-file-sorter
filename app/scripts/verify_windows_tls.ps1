param([Parameter(Mandatory)][string]$Directory, [switch]$RemoveLegacyBundle)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'windows_tls_layout.ps1')
if ($RemoveLegacyBundle) { Remove-AifsLegacyTlsFiles -Directory $Directory }
Assert-AifsNativeTlsLayout -Directory $Directory
