# 2025-12-04: chore(windows-dpi): add

## Covered commits
- `3123d15` `2025-12-04` `chore(windows-dpi): add`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/build_windows.ps1`
- `M` `app/lib/Utils.cpp`
- `M` `app/main.cpp`
- `M` `app/startapp_windows.cpp`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/build_windows.ps1`, `app/lib/Utils.cpp`, `app/main.cpp`, `app/startapp_windows.cpp`. It changed the repository support state, metadata, or supporting files in the way described by `chore(windows-dpi): add`.

Before this commit, the repository reflected the state immediately preceding `3123d15`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/build_windows.ps1 b/app/build_windows.ps1
--- a/app/build_windows.ps1
+++ b/app/build_windows.ps1
@@ -257,6 +257,31 @@ if (Test-Path $precompiledCpuBin) {
             Copy-Item $_.FullName -Destination $destWocuda -Force
         }
 }
+
+# Ensure MinGW/OpenBLAS runtime deps land in the CPU-only (wocuda) bundle.
+$mingwRuntimeNames = @("libgomp-1.dll", "libgcc_s_seh-1.dll", "libgfortran-5.dll", "libwinpthread-1.dll", "libquadmath-0.dll")
+$runtimeSearchPaths = @()
+if ($env:OPENBLAS_ROOT) {
+    $runtimeSearchPaths += (Join-Path $env:OPENBLAS_ROOT "bin")
+}
+$runtimeSearchPaths += "C:\msys64\mingw64\bin"
+
+foreach ($dllName in $mingwRuntimeNames) {
+    $found = $false
+    foreach ($path in $runtimeSearchPaths) {
+        if (-not (Test-Path $path)) { continue }
+        $candidate = Join-Path $path $dllName
+        if (Test-Path $candidate) {
+            Copy-Item $candidate -Destination $destWocuda -Force
+            $found = $true
+            break
+        }
+    }
+    if (-not $found) {
+        Write-Warning "Could not locate $dllName in any runtime path. Add it manually to $destWocuda if needed."
+    }
+}
+
 if (Test-Path $precompiledCudaBin) {
     Get-ChildItem -Path $precompiledCudaBin -Filter "*.dll" -File -ErrorAction SilentlyContinue |
         ForEach-Object {
diff --git a/app/lib/Utils.cpp b/app/lib/Utils.cpp
index 6df43e8..e8373dc 100644
--- a/app/lib/Utils.cpp
+++ b/app/lib/Utils.cpp
@@ -137,6 +137,10 @@ std::optional<std::string> strip_prefix(const std::string& path,
 #include <Types.hpp>
 #include <cstddef>
 #include <stdexcept>
+#ifdef _WIN32
+    #include <appmodel.h>
+    #include <cwchar>
+#endif
 
 // Shortcuts for loading libraries on different OSes
 #ifdef _WIN32
```

The excerpt is taken from the commit diff for `chore(windows-dpi): add`. The most relevant surfaces are `app/build_windows.ps1`, `app/lib/Utils.cpp`, `app/main.cpp`, `app/startapp_windows.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
