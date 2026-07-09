# 2025-10-29: Windows CMake transition, Linux build modernization, and file explorer resilience

## Covered commits
- `7bde92a` `2025-10-26` `feat(cmake): switch from make to cmake for Windows`
- `d8e3102` `2025-10-29` `feat(build-linux): upgrade the build process on Linux`
- `d58aa69` `2025-10-29` `feat(file-explorer): appearance of mounted network shares`
- `df9de0d` `2025-10-29` `fix(file-explorer): file structure now browsable outside of user's home`
- `c29ef94` `2025-10-29` `fix(app): fixed potentially error-prone areas`
- `b4e6647` `2025-10-29` `feat(splash-screen): improve`
- `c1b2343` `2025-10-28` `chore(build-windows): add exe icon in Qt6 environment`
- `d875dd3` `2025-10-28` `chore(build-windows): update for CMake`
- `daf5ded` `2025-10-28` `chore(local-llm): update code related to Qt6 to compile under Windows`

## Motivation
Once the Qt6 UI existed, the next pressure point was build and packaging reliability. The Windows build needed to move away from ad-hoc rules toward a CMake-based flow, Linux packaging needed modernization, and the file explorer needed to represent mounted/networked and non-home paths more accurately for real-world sorting targets.

## What changed
These commits introduced the Windows CMake path, improved Linux build structure, surfaced mounted network shares and non-home paths in the file explorer, and cleaned up fragile UI areas such as the splash-screen behavior and icon handling while the transition was still fresh.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `7bde92a`
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -175,7 +175,67 @@ File categorization with local LLMs is completely free of charge. If you prefer
 
 ### Windows
 
-Pre-built installers continue to be published on the [Releases](https://github.com/hyperfield/ai-file-sorter/releases) page. The new Qt6-based toolchain targets MSVC rather than MSYS2/GTK; native build instructions will be documented once the workflow is finalised.
+Build now targets native MSVC + Qt6 without MSYS2. Two options are supported; the vcpkg route is simplest.
+
+Option A - CMake + vcpkg (recommended)
+
+1. Install prerequisites:
+   - Visual Studio 2022 with Desktop C++ workload
+   - CMake 3.21+ (Visual Studio ships a recent version)
+   - vcpkg: https://github.com/microsoft/vcpkg (clone and bootstrap)
+2. Clone repo and submodules:
+   ```powershell
+   git clone https://github.com/hyperfield/ai-file-sorter.git
+   cd ai-file-sorter
+   git submodule update --init --recursive
+   ```
+3. Determine your vcpkg root. It is the folder that contains `vcpkg.exe` (for example `C:\dev\vcpkg`).
+    - If `vcpkg` is on your `PATH`, run this command to print the location:
+      ```powershell
+      Split-Path -Parent (Get-Command vcpkg).Source
+      ```
+    - Otherwise use the directory where you cloned vcpkg.
+4. Build the bundled `llama.cpp` runtime (run from the same **x64 Native Tools** / **VS 2022 Developer PowerShell** shell). Pass `cuda=on` if you have a CUDA toolkit configured, otherwise leave it off (default is CPU-only). The script installs OpenBLAS and cURL via vcpkg automatically if they are missing:
+   ```powershell
+   pwsh .\app\scripts\build_llama_windows.ps1 [cuda=on|off] [vcpkgroot=C:\dev\vcpkg]
+   ```
+   This script produces the `llama.dll`/`ggml*.dll` set under `app\lib\precompiled` which the GUI links against.
+5. Build the Qt6 application using the helper script (still in the VS shell). The helper stages runtime DLLs via `windeployqt`, so `app\bin` is immediately runnable:
+   ```powershell
+    # If script execution is blocked, run:  Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
+    pwsh .\app\build_windows.ps1 -VcpkgRoot "C:\dev\vcpkg"
+    ```
+    `-VcpkgRoot` is optional if you set `VCPKG_ROOT`/`VPKG_ROOT` or have `vcpkg`/`vpkg` on `PATH`.
+    The executable and all required Qt/third-party DLLs are placed in `app\bin`. Pass `-SkipDeploy` if you only want the binaries without bundling runtime DLLs.
+
+Option B - CMake + Qt online installer
+
+1. Install prerequisites:
+   - Visual Studio 2022 with Desktop C++ workload
+   - Qt 6.x MSVC kit via Qt Online Installer (e.g., Qt 6.6+ with MSVC 2019/2022)
+   - CMake 3.21+
+   - vcpkg (for non-Qt libs): curl, jsoncpp, sqlite3, openssl, fmt, spdlog, gettext
+2. Build the bundled `llama.cpp` runtime (same VS shell). Any missing OpenBLAS/cURL packages are installed automatically via vcpkg:
+   ```powershell
+   pwsh .\app\scripts\build_llama_windows.ps1 [cuda=on|off] [vcpkgroot=C:\dev\vcpkg]
+   ```
+   This is required before configuring the GUI because the build links against the produced `llama` static libraries/DLLs.
+3. Configure CMake to see Qt (adapt `CMAKE_PREFIX_PATH` to your Qt install):
+    ```powershell
+    $env:VCPKG_ROOT = "C:\path\to\vcpkg"
+    $qt = "C:\Qt\6.6.3\msvc2019_64"  # example
+    cmake -S app -B build -G "Ninja" `
+      -DCMAKE_PREFIX_PATH=$qt `
+     -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake `
+     -DVCPKG_TARGET_TRIPLET=x64-windows
+   cmake --build build --config Release
+   ```
+
+Notes
+- To rebuild from scratch, run `pwsh .\app\build_windows.ps1 -Clean`. The script removes the local `app\build-windows` directory before configuring.
+- Runtime DLLs are copied automatically via `windeployqt` after each successful build; skip this step with `-SkipDeploy` if you manage deployment yourself.
+- If Visual Studio sets `VCPKG_ROOT` to its bundled copy under `Program Files`, clone vcpkg to a writable directory (for example `C:\dev\vcpkg`) and pass `vcpkgroot=<path>` when running `build_llama_windows.ps1`.
+- If you enable CUDA for local models, build `llama.cpp` with CUDA first and reconfigure CMake accordingly.
 
 ---
 
diff --git a/app/CMakeLists.txt b/app/CMakeLists.txt
new file mode 100644
index 0000000..cd0b0de
--- /dev/null
+++ b/app/CMakeLists.txt
@@ -0,0 +1,146 @@
+cmake_minimum_required(VERSION 3.21)
+
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `d8e3102`
```diff
diff --git a/.gitignore b/.gitignore
--- a/.gitignore
+++ b/.gitignore
@@ -53,6 +53,7 @@ bin/
 reset_key_4_github.sh
 create_macos_bundle.sh
 create_dmg.sh
+package_deb.sh
 
 # References
 # includePaths.txt
diff --git a/README.md b/README.md
index 6955c7a..14ef87f 100644
--- a/README.md
+++ b/README.md
@@ -113,6 +113,24 @@ File categorization with local LLMs is completely free of charge. If you prefer
 
 ### Linux
 
+#### Prebuilt Debian/Ubuntu package
+
+1. **Install runtime prerequisites** (Qt6, networking, database, math libraries):
+   ```bash
+   sudo apt update && sudo apt install -y \
+     libqt6widgets6 libqt6gui6 libqt6core6 libqt6dbus6 \
+     libcurl4 libjsoncpp25 libfmt8 libopenblas0-pthread
+   ```
+   Ensure that the Qt platform plugins are installed (on Ubuntu 22.04 this is provided by `qt6-wayland`).
+   GPU acceleration additionally requires an NVIDIA driver with the matching CUDA runtime (`nvidia-cuda-toolkit` or the vendor packages).
+2. **Install the package**
+   ```bash
+   sudo apt install ./aifilesorter_1.0.0_amd64.deb
+   ```
+   Using `apt install` (rather than `dpkg -i`) ensures any missing dependencies listed above are installed automatically.
+
+#### Build from source
+
 1. **Install dependencies**
    - Debian / Ubuntu:
      ```bash
```

This second excerpt is included because `d8e3102` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
