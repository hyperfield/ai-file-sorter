# 2025-07-10: chore(build_llama_windows.ps1): improve and complete the llama.cpp build script for windows

## Covered commits
- `9a636f7` `2025-07-10` `chore(build_llama_windows.ps1): improve and complete the llama.cpp build script for windows`

## Motivation
This dependency-management commit kept bundled third-party code in sync with the capabilities or fixes the project needed. Those updates are usually required to unblock platform fixes, tests, or packaging changes in adjacent commits.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/scripts/build_llama_windows.ps1`

## What changed from what, why, and how
The commit updated dependency pointers or related build references in `app/scripts/build_llama_windows.ps1`. It moved the repository from older third-party revisions to newer ones needed by the surrounding feature or fix work.

Before this commit, the repository reflected the state immediately preceding `9a636f7`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/scripts/build_llama_windows.ps1 b/app/scripts/build_llama_windows.ps1
--- a/app/scripts/build_llama_windows.ps1
+++ b/app/scripts/build_llama_windows.ps1
@@ -1,5 +1,15 @@
 $ErrorActionPreference = "Stop"
 
+# Parse optional argument (cuda=on or cuda=off)
+$useCuda = "OFF"
+foreach ($arg in $args) {
+    if ($arg -match "^cuda=(on|off)$") {
+        $useCuda = ($Matches[1].ToUpper())
+    }
+}
+
+Write-Host "`nCUDA Support: $useCuda`n"
+
 $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
 $llamaDir = Join-Path $scriptDir "..\include\external\llama.cpp"
 
@@ -14,9 +24,12 @@ $headersDir = Join-Path $scriptDir "..\include\llama"
 
 Push-Location $llamaDir
 
-Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
-New-Item -ItemType Directory -Path build | Out-Null
+if (Test-Path "build") {
+    Remove-Item -Recurse -Force "build"
+}
+New-Item -ItemType Directory -Path "build" | Out-Null
 
+# CUDA paths (adjust if needed)
 $cudaRoot = "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.9"
 $includeDir = "$cudaRoot/include"
 $libDir = "$cudaRoot/lib/x64/cudart.lib"
```

The excerpt is taken from the commit diff for `chore(build_llama_windows.ps1): improve and complete the llama.cpp build script for windows`. The most relevant surfaces are `app/scripts/build_llama_windows.ps1`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
