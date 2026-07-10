# 2025-11-22: fix(llm-selection-dialog): fix

## Covered commits
- `6c150c9` `2025-11-22` `fix(llm-selection-dialog): fix`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `M` `app/include/external/llama.cpp`
- `M` `app/lib/LLMSelectionDialog.cpp`
- `M` `app/lib/MainAppUiBuilder.cpp`

## What changed from what, why, and how
The commit corrected behavior in `README.md`, `app/include/external/llama.cpp`, `app/lib/LLMSelectionDialog.cpp`, `app/lib/MainAppUiBuilder.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(llm-selection-dialog): fix`.

Before this commit, the repository reflected the state immediately preceding `6c150c9`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -222,6 +222,7 @@ Option A - CMake + vcpkg (recommended)
    - Visual Studio 2022 with Desktop C++ workload
    - CMake 3.21+ (Visual Studio ships a recent version)
    - vcpkg: <https://github.com/microsoft/vcpkg> (clone and bootstrap)
+   - **MSYS2 MinGW64 + OpenBLAS**: install MSYS2 from <https://www.msys2.org>, open an *MSYS2 MINGW64* shell, and run `pacman -S --needed mingw-w64-x86_64-openblas`. The `build_llama_windows.ps1` script uses this OpenBLAS copy for CPU-only builds (the vcpkg variant is not suitable), defaulting to `C:\msys64\mingw64` unless you pass `openblasroot=<path>` or set `OPENBLAS_ROOT`.
 2. Clone repo and submodules:
    ```powershell
    git clone https://github.com/hyperfield/ai-file-sorter.git
@@ -234,15 +235,15 @@ Option A - CMake + vcpkg (recommended)
       Split-Path -Parent (Get-Command vcpkg).Source
       ```
     - Otherwise use the directory where you cloned vcpkg.
-4. Build the bundled `llama.cpp` runtime variants (run from the same **x64 Native Tools** / **VS 2022 Developer PowerShell** shell). Invoke the script once per backend you need:
+4. Build the bundled `llama.cpp` runtime variants (run from the same **x64 Native Tools** / **VS 2022 Developer PowerShell** shell). Invoke the script once per backend you need. Make sure the MSYS2 OpenBLAS install from step 1 is present before running the CPU-only variant (or pass `openblasroot=<path>` explicitly):
    ```powershell
-  # CPU / OpenBLAS only
-  app\scripts\build_llama_windows.ps1 cuda=off vulkan=off vcpkgroot=C:\dev\vcpkg
-  # CUDA (requires matching NVIDIA toolkit/driver)
-  app\scripts\build_llama_windows.ps1 cuda=on vulkan=off vcpkgroot=C:\dev\vcpkg
-  # Vulkan (requires LunarG Vulkan SDK or vendor Vulkan 1.2+ runtime)
-  app\scripts\build_llama_windows.ps1 cuda=off vulkan=on vcpkgroot=C:\dev\vcpkg
-  ```
+   # CPU / OpenBLAS only
+   app\scripts\build_llama_windows.ps1 cuda=off vulkan=off vcpkgroot=C:\dev\vcpkg
+   # CUDA (requires matching NVIDIA toolkit/driver)
+   app\scripts\build_llama_windows.ps1 cuda=on vulkan=off vcpkgroot=C:\dev\vcpkg
+   # Vulkan (requires LunarG Vulkan SDK or vendor Vulkan 1.2+ runtime)
+   app\scripts\build_llama_windows.ps1 cuda=off vulkan=on vcpkgroot=C:\dev\vcpkg
+   ```
   Each run emits the appropriate `llama.dll` / `ggml*.dll` pair under `app\lib\precompiled\<cpu|cuda|vulkan>` and copies the runtime DLLs into `app\lib\ggml\w<variant>`. For Vulkan builds, install the latest LunarG Vulkan SDK (or the vendor's runtime), ensure `vulkaninfo` succeeds in the same shell, and then run the script. Supplying both Vulkan and (optionally) CUDA artifacts lets `StartAiFileSorter.exe` detect the best backend at launch—Vulkan is preferred, CUDA is used when Vulkan is missing, and CPU remains the fallback, so CUDA is not required.
 5. Build the Qt6 application using the helper script (still in the VS shell). The helper stages runtime DLLs via `windeployqt`, so `app\build-windows\Release` is immediately runnable:
    ```powershell
```

The excerpt is taken from the commit diff for `fix(llm-selection-dialog): fix`. The most relevant surfaces are `README.md`, `app/include/external/llama.cpp`, `app/lib/LLMSelectionDialog.cpp`, `app/lib/MainAppUiBuilder.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
