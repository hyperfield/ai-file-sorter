<!-- markdownlint-disable MD046 -->
# AI File Sorter

[![Code Version](https://img.shields.io/badge/Code-1.4.5-blue)](#)
[![Release Version](https://img.shields.io/github/v/release/hyperfield/ai-file-sorter?label=Release)](#)
[![SourceForge Downloads](https://img.shields.io/sourceforge/dt/ai-file-sorter.svg?label=SourceForge%20downloads)](https://sourceforge.net/projects/ai-file-sorter/files/latest/download)
[![SourceForge Downloads](https://img.shields.io/sourceforge/dw/ai-file-sorter.svg?label=SourceForge%20downloads)](https://sourceforge.net/projects/ai-file-sorter/files/latest/download)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/2c646c836a9844be964fbf681649c3cd)](https://app.codacy.com/gh/hyperfield/ai-file-sorter/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)
[![Donate](https://img.shields.io/badge/Support%20AI%20File%20Sorter-orange)](https://filesorter.app/donate)

<p align="center">
  <img src="app/resources/images/icon_256x256.png" alt="AI File Sorter logo" width="128" height="128">
</p>

<p align="center">
  <img src="images/platform-logos/logo-vulkan.png" alt="Vulkan" width="160">
  <img src="images/platform-logos/logo-cuda.png" alt="CUDA" width="160">
  <img src="images/platform-logos/logo-metal.png" alt="Apple Metal" width="160">
  <img src="images/platform-logos/logo-windows.png" alt="Windows" width="160">
  <img src="images/platform-logos/logo-macos.png" alt="macOS" width="160">
  <img src="images/platform-logos/logo-linux.png" alt="Linux" width="160">
</p>

AI File Sorter is a powerful, cross-platform desktop application that automates file organization with the help of AI.

It helps tidy up cluttered folders like Downloads, external drives, or NAS storage by automatically categorizing files based on their names, extensions, directory context, taxonomy, and other heuristics for accuracy and consistency.

The app uses a **taxonomy-based system**, which essentially means that it builds up a smarter internal reference for your file types and naming patterns.

The app intelligently assigns categories and optional subcategories, which you can review and adjust before confirming. Once approved, the necessary folders are created and your files are sorted automatically.  

AI File Sorter runs **local large language models (LLMs)** such as *LLaMa 3B* and *Mistral 7B*, and does not require an internet connection unless you choose to use a remote model.

File content–based sorting for certain file types is also in development.

---

#### How It Works

1. Point it at a folder or drive  
2. It runs a local LLM to analyze your files  
3. The LLM suggests categorizations  
4. You review and adjust if needed - done  

---

[![Download ai-file-sorter](https://a.fsdn.com/con/app/sf-download-button)](https://sourceforge.net/projects/ai-file-sorter/files/latest/download)

![AI File Sorter Screenshot](images/screenshots/ai-file-sorter-win.gif) ![AI File Sorter Screenshot](images/screenshots/main_windows_macos.png) ![AI File Sorter Screenshot](images/screenshots/categorization-dialog-macos.png)

---

- [AI File Sorter](#ai-file-sorter)
  - [Changelog](#changelog)
  - [Features](#features)
  - [Categorization](#categorization)
    - [Categorization modes](#categorization-modes)
    - [Category whitelists](#category-whitelists)
  - [Requirements](#requirements)
  - [Installation](#installation)
    - [Linux](#linux)
    - [macOS](#macos)
    - [Windows](#windows)
  - [Uninstallation](#uninstallation)
  - [Using your OpenAI API key](#using-your-openai-api-key)
  - [Using your Gemini API key](#using-your-gemini-api-key)
  - [Testing](#testing)
  - [How to Use](#how-to-use)
  - [Sorting a Remote Directory (e.g., NAS)](#sorting-a-remote-directory-eg-nas)
  - [Contributing](#contributing)
  - [Credits](#credits)
  - [License](#license)
  - [Donation](#donation)

---

## Changelog

## [1.4.0] - 2025-12-30
- Added dry run / preview-only mode with From→To table, no moves performed until you uncheck.
- Persistent Undo: the latest sort saves a plan file; use Edit → “Undo last run” even after closing dialogs.
- UI tweaks: Name column auto-resizes, new translations for dry run/undo strings, Undo moved to top of Edit menu.
- A few more guard rails added.

See [CHANGELOG.md](CHANGELOG.md) for the full history.

---

## Features

- **AI-Powered Categorization**: Classify files intelligently using either a **local LLM** (LLaMa, Mistral) or a remote model (ChatGPT with your own OpenAI API key, or Gemini with your own Gemini API key).
- **Offline-Friendly**: Use a local LLM to categorize files entirely - no internet or API key required.
  **Robust Categorization Algorithm**: Consistency across categories is supported by taxonomy and heuristics.
  **Customizable Sorting Rules**: Automatically assign categories and subcategories for granular organization.
- **Two categorization modes**: Pick **More Refined** for detailed labels or **More Consistent** to bias toward uniform categories within a folder.
- **Category whitelists**: Define named whitelists of allowed categories/subcategories, manage them under **Settings → Manage category whitelists…**, and toggle/select them in the main window when you want to constrain model output for a session.
- **Multilingual categorization**: Have the LLM assign categories in Dutch, French, German, Italian, Polish, Portuguese, Spanish, or Turkish (model dependent).
- **Custom local LLMs**: Register your own local GGUF models directly from the **Select LLM** dialog.
- **Sortable review**: Sort the Categorization Review table by file name, category, or subcategory to triage faster.
- **Qt6 Interface**: Lightweight and responsive UI with refreshed menus and icons.
- **Cross-Platform Compatibility**: Works on Windows, macOS, and Linux.
- **Local Database Caching**: Speeds up repeated categorization and minimizes remote LLM usage costs.
- **Sorting Preview**: See how files will be organized before confirming changes.
- 🧪 **Dry run** / preview-only mode to inspect planned moves without touching files.
- ↩️ **Persistent Undo** ("Undo last run") even after closing the sort dialog.
- **Bring your own key**: Paste your OpenAI or Gemini API key once; it's stored locally and reused for remote runs.
- **Update Notifications**: Get notified about updates - with optional or required update flows.

---

## Categorization

### Categorization modes

- **More refined**: The flexible, detail-oriented mode. Consistency hints are disabled so the model can pick the most specific category/subcategory it deems appropriate, which is useful for long-tail or mixed folders.
- **More consistent**: The uniform mode. The model receives consistency hints from prior assignments in the current session so files with similar names/extensions trend toward the same categories. This is helpful when you want strict uniformity across a batch.
- Switch between the two via the **Categorization type** radio buttons on the main window; your choice is saved for the next run.

### Category whitelists

- Enable **Use a whitelist** to inject the selected whitelist into the LLM prompt; disable it to let the model choose freely.
- Manage lists (add, edit, remove) under **Settings → Manage category whitelists…**. A default list is auto-created only when no lists exist, and multiple named lists can be kept for different projects.
- Keep each whitelist to roughly **15–20 categories/subcategories** to avoid overlong prompts on smaller local models. Use several narrower lists instead of a single very long one.
- Whitelists apply in either categorization mode; pair them with **More consistent** when you want the strongest adherence to a constrained vocabulary.

---

## Requirements

- **Operating System**: Linux or macOS for source builds (Windows builds are provided as binaries; native Qt/MSVC build instructions are planned).
- **Compiler**: A C++20-capable compiler (`g++` or `clang++`).
- **Qt 6**: Core, Gui, Widgets modules and the Qt resource compiler (`qt6-base-dev` / `qt6-tools` on Linux, `brew install qt` on macOS).
- **Libraries**: `curl`, `sqlite3`, `fmt`, `spdlog`, and the prebuilt `llama` libraries shipped under `app/lib/precompiled`.
- **Optional GPU backends**: A Vulkan 1.2+ runtime (preferred) or CUDA 12.x for NVIDIA cards. `StartAiFileSorter.exe`/`run_aifilesorter.sh` auto-detect the best available backend and fall back to CPU/OpenBLAS automatically, so CUDA is never required to run the app.
- **Git** (optional): For cloning this repository. Archives can also be downloaded.
- **OpenAI or Gemini API key** (optional): Required only when using the remote ChatGPT or Gemini workflow.

---

## Installation

File categorization with local LLMs is completely free of charge. If you prefer to use a remote workflow (ChatGPT or Gemini) you will need your own API key with a small balance or within the free tier (see [Using your OpenAI API key](#using-your-openai-api-key) or [Using your Gemini API key](#using-your-gemini-api-key)).

### Linux

#### Prebuilt Debian/Ubuntu package

1. **Install runtime prerequisites** (Qt6, networking, database, math libraries):
   ```bash
   sudo apt update && sudo apt install -y \
     libqt6widgets6 libcurl4 libjsoncpp25 libfmt9 libopenblas0-pthread
   ```
   Ensure that the Qt platform plugins are installed (on Ubuntu 22.04 this is provided by `qt6-wayland`).
   GPU acceleration additionally requires either a working Vulkan 1.2+ stack (Mesa, AMD/Intel/NVIDIA drivers) or, for NVIDIA users, the matching CUDA runtime (`nvidia-cuda-toolkit` or vendor packages). The launcher automatically prefers Vulkan when both are present and falls back to CPU if neither is available.
2. **Install the package**
   ```bash
   sudo apt install ./aifilesorter_1.0.0_amd64.deb
   ```
   Using `apt install` (rather than `dpkg -i`) ensures any missing dependencies listed above are installed automatically.

#### Build from source

1. **Install dependencies**
   - Debian / Ubuntu:
     ```bash
     sudo apt update && sudo apt install -y \
       build-essential cmake git qt6-base-dev qt6-base-dev-tools qt6-tools-dev-tools \
       libcurl4-openssl-dev libjsoncpp-dev libsqlite3-dev libssl-dev libfmt-dev libspdlog-dev
     ```
   - Fedora / RHEL:

     ```bash
     export PATH="/usr/lib64/qt6/libexec:$PATH"
     sudo mkdir -p /usr/include/jsoncpp/json
     sudo ln -s /usr/include/json/json.h /usr/include/jsoncpp/json/json.h
     ```

     ```bash
     sudo dnf install -y gcc-c++ cmake git qt6-qtbase-devel qt6-qttools-devel \
       libcurl-devel jsoncpp-devel sqlite-devel openssl-devel fmt-devel spdlog-devel
     ```

   - Arch / Manjaro:

     ```bash
     sudo pacman -S --needed base-devel git cmake qt6-base qt6-tools curl jsoncpp sqlite openssl fmt spdlog
     ```

     Optional GPU acceleration also requires either the distro Vulkan 1.2+ driver/runtime (Mesa, AMD, Intel, NVIDIA) or CUDA packages for NVIDIA cards. Install whichever stack you plan to use; the app will fall back to CPU automatically if none are detected.

2. **Clone the repository**

   ```bash
   git clone https://github.com/hyperfield/ai-file-sorter.git
   cd ai-file-sorter
   git submodule update --init --recursive --remote
   ```

   > **Submodule tip:** If you previously downloaded `llama.cpp` or Catch2 manually, remove or rename `app/include/external/llama.cpp` and `external/Catch2` before running the `git submodule` command. Git needs those directories to be empty so it can populate them with the tracked submodules.

3. **Build the llama runtime variants** (run once per backend you plan to ship/test)

   ```bash
   # CPU / OpenBLAS
   ./app/scripts/build_llama_linux.sh cuda=off vulkan=off
   # CUDA (optional; requires NVIDIA driver + CUDA toolkit)
   ./app/scripts/build_llama_linux.sh cuda=on vulkan=off
   # Vulkan (optional; requires a working Vulkan 1.2+ stack, e.g. mesa-vulkan-drivers + vulkan-tools)
   ./app/scripts/build_llama_linux.sh cuda=off vulkan=on
   ```

   Each invocation stages the corresponding `llama`/`ggml` libraries under `app/lib/precompiled/<variant>` and the runtime DLL/SO copies under `app/lib/ggml/w<variant>`. The script refuses to enable CUDA and Vulkan simultaneously, so run it separately for each backend. Shipping both directories lets the launcher pick Vulkan when available, then CUDA, and otherwise stay on CPU—no CUDA-only dependency remains.

4. **Compile the application**

   ```bash
   cd app
   make -j4
   ```

   The binary is produced at `app/bin/aifilesorter`.

5. **Install system-wide (optional)**

   ```bash
   sudo make install
   ```

### macOS

1. **Install Xcode command-line tools** (`xcode-select --install`).
2. **Install Homebrew** (if required).
3. **Install dependencies**

   ```bash
   brew install qt curl jsoncpp sqlite openssl fmt spdlog cmake git pkgconfig libffi
   ```

   Add Qt to your environment if it is not already present:

   ```bash
   export PATH="$(brew --prefix)/opt/qt/bin:$PATH"
   export PKG_CONFIG_PATH="$(brew --prefix)/lib/pkgconfig:$(brew --prefix)/share/pkgconfig:$PKG_CONFIG_PATH"
   ```

4. **Clone the repository and submodules** (same commands as Linux).
   > The macOS build pins `MACOSX_DEPLOYMENT_TARGET=11.0` so the Mach-O `LC_BUILD_VERSION` covers Apple Silicon and newer releases (including Sequoia). Raise or lower it (e.g., `export MACOSX_DEPLOYMENT_TARGET=15.0`) if you need a different floor.

5. **Build the llama runtime (Metal-only on macOS)**

   ```bash
   ./app/scripts/build_llama_macos.sh
   ```

   The macOS helper already produces the Metal-enabled variant the app needs, so no extra GPU-specific invocations are required on this platform.

6. **Compile the application**

   ```bash
   cd app
   make -j4
   sudo make install   # optional
   ```

   > **Fix for the 1.1.0 macOS build:** That package shipped with `LC_BUILD_VERSION` set to macOS 26.0, which Sequoia blocks. If you still have that build, you can patch it in place:

   > ```bash
   > APP="/Applications/AI File Sorter.app"
   > BIN="$APP/Contents/MacOS/aifilesorter"
   > vtool -replace -set-build-version macos 11.0 11.0 -output "$BIN.patched" "$BIN" && mv "$BIN.patched" "$BIN"
   > codesign --force --deep --sign - "$APP"
   > xattr -d com.apple.quarantine "$APP" || true
   > ```

   > (`vtool` ships with the Xcode command line tools.) Future releases are built with the corrected deployment target.

### Windows

Build now targets native MSVC + Qt6 without MSYS2. Two options are supported; the vcpkg route is simplest.

Option A - CMake + vcpkg (recommended)

1. Install prerequisites:
   - Visual Studio 2022 with Desktop C++ workload
   - CMake 3.21+ (Visual Studio ships a recent version)
   - vcpkg: <https://github.com/microsoft/vcpkg> (clone and bootstrap)
   - **MSYS2 MinGW64 + OpenBLAS**: install MSYS2 from <https://www.msys2.org>, open an *MSYS2 MINGW64* shell, and run `pacman -S --needed mingw-w64-x86_64-openblas`. The `build_llama_windows.ps1` script uses this OpenBLAS copy for CPU-only builds (the vcpkg variant is not suitable), defaulting to `C:\msys64\mingw64` unless you pass `openblasroot=<path>` or set `OPENBLAS_ROOT`.
2. Clone repo and submodules:

   ```powershell
   git clone https://github.com/hyperfield/ai-file-sorter.git
   cd ai-file-sorter
   git submodule update --init --recursive
   ```

3. Determine your vcpkg root. It is the folder that contains `vcpkg.exe` (for example `C:\dev\vcpkg`).
    - If `vcpkg` is on your `PATH`, run this command to print the location:

      ```powershell
      Split-Path -Parent (Get-Command vcpkg).Source
      ```

    - Otherwise use the directory where you cloned vcpkg.
4. Build the bundled `llama.cpp` runtime variants (run from the same **x64 Native Tools** / **VS 2022 Developer PowerShell** shell). Invoke the script once per backend you need. Make sure the MSYS2 OpenBLAS install from step 1 is present before running the CPU-only variant (or pass `openblasroot=<path>` explicitly):

   ```powershell
   # CPU / OpenBLAS only
   app\scripts\build_llama_windows.ps1 cuda=off vulkan=off vcpkgroot=C:\dev\vcpkg
   # CUDA (requires matching NVIDIA toolkit/driver)
   app\scripts\build_llama_windows.ps1 cuda=on vulkan=off vcpkgroot=C:\dev\vcpkg
   # Vulkan (requires LunarG Vulkan SDK or vendor Vulkan 1.2+ runtime)
   app\scripts\build_llama_windows.ps1 cuda=off vulkan=on vcpkgroot=C:\dev\vcpkg
   ```
  
  Each run emits the appropriate `llama.dll` / `ggml*.dll` pair under `app\lib\precompiled\<cpu|cuda|vulkan>` and copies the runtime DLLs into `app\lib\ggml\w<variant>`. For Vulkan builds, install the latest LunarG Vulkan SDK (or the vendor's runtime), ensure `vulkaninfo` succeeds in the same shell, and then run the script. Supplying both Vulkan and (optionally) CUDA artifacts lets `StartAiFileSorter.exe` detect the best backend at launch—Vulkan is preferred, CUDA is used when Vulkan is missing, and CPU remains the fallback, so CUDA is not required.

5. Build the Qt6 application using the helper script (still in the VS shell). The helper stages runtime DLLs via `windeployqt`, so `app\build-windows\Release` is immediately runnable:

   ```powershell
   # One-time per shell if script execution is blocked:
   Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass

   app\build_windows.ps1 -Configuration Release -VcpkgRoot C:\dev\vcpkg
   ```

   - Replace `C:\dev\vcpkg` with the path where you cloned vcpkg; it must contain `scripts\buildsystems\vcpkg.cmake`.
   - Always launch the app via `StartAiFileSorter.exe`. This small bootstrapper configures the GGML/CUDA/Vulkan DLLs, auto-selects Vulkan → CUDA → CPU at runtime, and sets the environment before spawning `aifilesorter.exe`. Launching `aifilesorter.exe` directly now shows a reminder dialog; developers can bypass it (for debugging) by adding `--allow-direct-launch` when invoking the GUI manually.
   - `-VcpkgRoot` is optional if `VCPKG_ROOT`/`VPKG_ROOT` is set or `vcpkg`/`vpkg` is on `PATH`.
   - The executable and required Qt/third-party DLLs are placed in `app\build-windows\Release`. Pass `-SkipDeploy` if you only want the binaries without bundling runtime DLLs.
   - Pass `-Parallel <N>` to override the default “all cores” parallel build behaviour (for example, `-Parallel 8`). By default the script invokes `cmake --build … --parallel <core-count>` and `ctest -j <core-count>` to keep both MSBuild and Ninja fully utilized.

Option B - CMake + Qt online installer

1. Install prerequisites:
   - Visual Studio 2022 with Desktop C++ workload
   - Qt 6.x MSVC kit via Qt Online Installer (e.g., Qt 6.6+ with MSVC 2019/2022)
   - CMake 3.21+
   - vcpkg (for non-Qt libs): curl, jsoncpp, sqlite3, openssl, fmt, spdlog, gettext
2. Build the bundled `llama.cpp` runtime (same VS shell). Any missing OpenBLAS/cURL packages are installed automatically via vcpkg:

   ```powershell
   pwsh .\app\scripts\build_llama_windows.ps1 [cuda=on|off] [vulkan=on|off] [vcpkgroot=C:\dev\vcpkg]
   ```

   This is required before configuring the GUI because the build links against the produced `llama` static libraries/DLLs.
3. Configure CMake to see Qt (adapt `CMAKE_PREFIX_PATH` to your Qt install):

    ```powershell
    $env:VCPKG_ROOT = "C:\path\to\vcpkg" (e.g., `C:\dev\vcpkg`)
    $qt = "C:\Qt\6.6.3\msvc2019_64"  # example
    cmake -S . -B build -G "Ninja" `
      -DCMAKE_PREFIX_PATH=$qt `
     -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake `
     -DVCPKG_TARGET_TRIPLET=x64-windows
   cmake --build build --config Release
   ```

Notes

- To rebuild from scratch, run `.\app\build_windows.ps1 -Clean`. The script removes the local `app\build-windows` directory before configuring.
- Runtime DLLs are copied automatically via `windeployqt` after each successful build; skip this step with `-SkipDeploy` if you manage deployment yourself.
- If Visual Studio sets `VCPKG_ROOT` to its bundled copy under `Program Files`, clone vcpkg to a writable directory (for example `C:\dev\vcpkg`) and pass `vcpkgroot=<path>` when running `build_llama_windows.ps1`.
- If you plan to ship CUDA or Vulkan acceleration, run the `build_llama_*` helper for each backend you intend to include before configuring CMake so the libraries exist. The runtime can carry both and auto-select at launch, so CUDA remains optional.

### Running tests

Catch2-based unit tests are optional. Enable them via CMake:

```bash
cmake -S app -B build-tests -DAI_FILE_SORTER_BUILD_TESTS=ON
cmake --build build-tests --target ai_file_sorter_tests
ctest --test-dir build-tests --output-on-failure
```

On Windows you can pass `-BuildTests` (and `-RunTests` to execute `ctest`) to `app\build_windows.ps1`:

```powershell
app\build_windows.ps1 -Configuration Release -BuildTests -RunTests
```

The current suite (under `tests/unit`) focuses on core utilities; expand it as new functionality gains coverage.

### Selecting a backend at runtime

Both the Linux launcher (`app/bin/run_aifilesorter.sh` / `aifilesorter-bin`) and the Windows starter accept the following optional flags:

- `--cuda={on|off}` – force-enable or disable the CUDA backend.
- `--vulkan={on|off}` – force-enable or disable the Vulkan backend.

When no flags are provided the app auto-detects available runtimes in priority order (Vulkan → CUDA → CPU). Use the flags to skip a backend (`--cuda=off` forces Vulkan/CPU even if CUDA is installed, `--vulkan=off` tests CUDA explicitly) or to validate a newly installed stack (`--vulkan=on`). Passing `on` to both flags is rejected, and if neither GPU backend is detected the app automatically stays on CPU.

---

## Uninstallation

- **Linux**: `cd app && sudo make uninstall`
- **macOS**: `cd app && sudo make uninstall`

The command removes the executable and the staged precompiled libraries. You can also delete cached local LLM models in `~/.local/share/aifilesorter/llms` (Linux) or `~/Library/Application Support/aifilesorter/llms` (macOS) if you no longer need them.

---

## Using your OpenAI API key

Want to use ChatGPT instead of the bundled local models? Bring your own OpenAI API key:

1. Open **Settings -> Select LLM** in the app.
2. Choose **ChatGPT (OpenAI API key)**, paste your key, and enter the ChatGPT model you want to use (for example `gpt-4o-mini`, `gpt-4.1`, or `o3-mini`).
3. Click **OK**. The key is stored locally in your AI File Sorter config (`config.ini` in the app data folder) and reused for future runs. Clear the field to remove it.
4. An internet connection is only required while this option is selected.

> The app no longer embeds a bundled key; you always provide your own OpenAI key.

---

## Using your Gemini API key

Prefer Google's models? Use your own Gemini API key:

1. Visit **https://aistudio.google.com** and sign in with your Google account.
2. In the left navigation, open **API keys** (or **Get API key**) and click **Create API key**. Choose *Create API key in new project* (or select an existing project) and copy the generated key.
3. In the app, open **Settings -> Select LLM**, choose **Gemini (Google AI Studio API key)**, paste your key, and enter the Gemini model you want (for example `gemini-2.5-flash-lite`, `gemini-2.5-flash`, or `gemini-2.5-pro`).
4. Click **OK**. The key is stored locally in your AI File Sorter config and reused for future runs. Clear the field to remove it.

> AI Studio keys can be used on the free tier until you hit Google’s limits; higher quotas or enterprise use require billing via Google Cloud.
> The app calls the Gemini `v1` `generateContent` endpoint; use model IDs from `https://generativelanguage.googleapis.com/v1/models?key=YOUR_KEY`. You can enter them with or without the leading `models/` prefix.

---

## Testing

- From the repo root, clean any old cache and run the CTest wrapper:
  
  ```bash
  cd app
  rm -rf ../build-tests      # clear a cache from another checkout
  ./scripts/rebuild_and_test.sh
  ```

- The script configures to `../build-tests`, builds, then runs `ctest`.
- If you have multiple copies of the repo (e.g., `ai-file-sorter` and `ai-file-sorter-mac-dist`), each needs its own `build-tests` folder; reusing one from a different path will make CMake complain about mismatched source/build directories.

---

## How to Use

1. Launch the application (see the last step in [Installation](#installation) according your OS).
2. Select a directory to analyze.

### Using dry run and undo

- In the results dialog, you can enable **"Dry run (preview only, do not move files)"** to preview planned moves. A preview dialog shows From/To without moving any files.
- After a real sort, the app saves a persistent undo plan. You can revert later via **Edit → "Undo last run"** (best-effort; skips conflicts/changes).

3. Tick off the checkboxes on the main window according to your preferences.
4. Click the **"Analyze"** button. The app will scan each file and/or directory based on your selected options.
5. A review dialog will appear. Verify the assigned categories (and subcategories, if enabled in step 3).
6. Click **"Confirm & Sort!"** to move the files, or **"Continue Later"** to postpone. You can always resume where you left off since categorization results are saved.

---

## Sorting a Remote Directory (e.g., NAS)

Follow the steps in [How to Use](#how-to-use), but modify **step 2** as follows:  

- **Windows:** Assign a drive letter (e.g., `Z:` or `X:`) to your network share ([instructions here](https://support.microsoft.com/en-us/windows/map-a-network-drive-in-windows-29ce55d1-34e3-a7e2-4801-131475f9557d)).  
- **Linux & macOS:** Mount the network share to a local folder using a command like:  

  ```sh
  sudo mount -t cifs //192.168.1.100/shared_folder /mnt/nas -o username=myuser,password=mypass,uid=$(id -u),gid=$(id -g)
  ```

(Replace 192.168.1.100/shared_folder with your actual network location path and adjust options as needed.)

---

## Contributing

- Fork the repository and submit pull requests.
- Report issues or suggest features on the GitHub issue tracker.
- Follow the existing code style and documentation format.

---

## Credits

- Curl: <https://github.com/curl/curl>
- Dotenv: <https://github.com/motdotla/dotenv>
- git-scm: <https://git-scm.com>
- Hugging Face: <https://huggingface.co>
- JSONCPP: <https://github.com/open-source-parsers/jsoncpp>
- LLaMa: <https://www.llama.com>
- llama.cpp <https://github.com/ggml-org/llama.cpp>
- Mistral AI: <https://mistral.ai>
- OpenAI: <https://platform.openai.com/docs/overview>
- OpenSSL: <https://github.com/openssl/openssl>
- Qt: <https://www.qt.io/>
- spdlog: <https://github.com/gabime/spdlog>

## License

This project is licensed under the GNU AFFERO GENERAL PUBLIC LICENSE (GNU AGPL). See the [LICENSE](LICENSE) file for details, or https://www.gnu.org/licenses/agpl-3.0.html.

---

## Donation

Support the development of **AI File Sorter** and its future features. Every contribution counts!

- **[Donate via PayPal](https://www.paypal.com/donate/?hosted_button_id=Z3XYTG38C62HQ)**
- **Bitcoin**: 12H8VvRG9PGyHoBzbYxVGcu8PaLL6pc3NM
- **Ethereum**: 0x09c6918160e2AA2b57BfD40BCF2A4BD61B38B2F9
- **Tron**: TGPr8b5RxC5JEaZXkzeGVxq7hExEAi7Yaj

USDT is also accepted in Ethereum and Tron chains.

---
