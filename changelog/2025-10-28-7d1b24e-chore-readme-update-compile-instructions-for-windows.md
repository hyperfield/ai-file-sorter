# 2025-10-28: chore(readme): update compile instructions for Windows

## Covered commits
- `7d1b24e` `2025-10-28` `chore(readme): update compile instructions for Windows`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `7d1b24e`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -197,13 +197,13 @@ Option A - CMake + vcpkg (recommended)
     - Otherwise use the directory where you cloned vcpkg.
 4. Build the bundled `llama.cpp` runtime (run from the same **x64 Native Tools** / **VS 2022 Developer PowerShell** shell). Pass `cuda=on` if you have a CUDA toolkit configured, otherwise leave it off (default is CPU-only). The script installs OpenBLAS and cURL via vcpkg automatically if they are missing:
    ```powershell
-   pwsh .\app\scripts\build_llama_windows.ps1 [cuda=on|off] [vcpkgroot=C:\dev\vcpkg]
+   app\scripts\build_llama_windows.ps1 [cuda=on|off] [vcpkgroot=C:\dev\vcpkg]
    ```
    This script produces the `llama.dll`/`ggml*.dll` set under `app\lib\precompiled` which the GUI links against.
 5. Build the Qt6 application using the helper script (still in the VS shell). The helper stages runtime DLLs via `windeployqt`, so `app\bin` is immediately runnable:
    ```powershell
     # If script execution is blocked, run:  Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
-    pwsh .\app\build_windows.ps1 -VcpkgRoot "C:\dev\vcpkg"
+    app\build_windows.ps1 -Configuration Release -VcpkgRoot C:\dev\vcpkg
     ```
     `-VcpkgRoot` is optional if you set `VCPKG_ROOT`/`VPKG_ROOT` or have `vcpkg`/`vpkg` on `PATH`.
     The executable and all required Qt/third-party DLLs are placed in `app\bin`. Pass `-SkipDeploy` if you only want the binaries without bundling runtime DLLs.
@@ -222,7 +222,7 @@ Option B - CMake + Qt online installer
    This is required before configuring the GUI because the build links against the produced `llama` static libraries/DLLs.
 3. Configure CMake to see Qt (adapt `CMAKE_PREFIX_PATH` to your Qt install):
     ```powershell
-    $env:VCPKG_ROOT = "C:\path\to\vcpkg"
+    $env:VCPKG_ROOT = "C:\path\to\vcpkg" (e.g., `C:\dev\vcpkg`)
     $qt = "C:\Qt\6.6.3\msvc2019_64"  # example
     cmake -S app -B build -G "Ninja" `
       -DCMAKE_PREFIX_PATH=$qt `
```

The excerpt is taken from the commit diff for `chore(readme): update compile instructions for Windows`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
