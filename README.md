<!-- markdownlint-disable MD046 -->
# AI File Sorter

[![Code Version](https://img.shields.io/badge/Code-1.8.0-blue)](#)
[![Release Version](https://img.shields.io/github/v/release/hyperfield/ai-file-sorter?label=Release)](#)
![filesorter.app Downloads](https://filesorter.app/download-stats/badge.svg)
[![SourceForge Downloads](https://img.shields.io/sourceforge/dt/ai-file-sorter.svg?label=SourceForge%20downloads)](https://sourceforge.net/projects/ai-file-sorter/files/latest/download)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/2c646c836a9844be964fbf681649c3cd)](https://app.codacy.com/gh/hyperfield/ai-file-sorter/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)
[![Donate](https://img.shields.io/badge/Support%20AI%20File%20Sorter-orange)](https://filesorter.app/donate/)

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

AI File Sorter is a cross-platform desktop application that uses AI to organize files and suggest cleaner, more consistent names for images, documents, and supported audio/video files. It is designed to reduce clutter, improve consistency, and make files easier to find later, whether for review, archiving, or long-term storage.

<p align="center">
  <img src="images/screenshots/before-after/aifs_before_after_v.png" alt="AI File Sorter before and after organization example" width="600">
</p>

The app can analyze picture files locally and suggest meaningful, human-readable names. For example, a generic file like IMG_2048.jpg can be renamed to something descriptive such as clouds_over_lake.jpg. It can also analyze supported document files and propose clearer names based on their text content. AI File Sorter can also clean up messy audio and video filenames by using the metadata already stored inside supported media files. If tags such as year, artist, album, or title are available, the app can turn them into a clear suggestion like `2024_artist_album_title.mp3`, which you can review, edit, or ignore before any change is applied.

AI File Sorter helps tidy up cluttered folders such as Downloads, external drives, or NAS storage by automatically grouping files based on their names, extensions, folder context, and learned organization patterns.

Instead of relying on fixed rules, the app gradually builds an internal understanding of how your files are typically organized and named. This allows it to make more consistent categorization and naming suggestions over time, while still letting you review and adjust everything before anything is applied.

Categories (and optional subcategories) are suggested for each file, and for supported file types, rename suggestions are provided as well. Once you confirm, the required folders are created automatically and files are sorted accordingly.

Privacy-first by design:
AI File Sorter can run entirely on your device, using local AI models such as Llama 3B (Q4) and Mistral 7B. No files, filenames, images, or metadata are uploaded anywhere, and no telemetry is sent. An internet connection is only needed if you explicitly choose to enable a remote model.

---

#### How It Works

1. Point the app at a folder or drive  
2. Files (and image content, when applicable) are analyzed using the selected local or remote model  
3. Category and rename suggestions are generated  
4. You review and adjust if needed - done  

---

[![Download ai-file-sorter](https://a.fsdn.com/con/app/sf-download-button)](https://sourceforge.net/projects/ai-file-sorter/files/latest/download)

[![Get it from Microsoft](https://get.microsoft.com/images/en-us%20dark.svg)](https://apps.microsoft.com/detail/9npk4dzd6r6s)

![AI File Sorter Screenshot](images/screenshots/ai-file-sorter-win.gif) ![AI File Sorter Screenshot](images/screenshots/main_windows_macos.png) ![AI File Sorter Screenshot](images/screenshots/sort-confirm-moved-win.png)

---

- [AI File Sorter](#ai-file-sorter)
  - [Changelog](#changelog)
  - [Features](#features)
  - [Categorization](#categorization)
    - [Categorization modes](#categorization-modes)
    - [Category whitelists](#category-whitelists)
  - [Image analysis (Visual LLM)](#image-analysis-visual-llm)
    - [Required visual LLM files](#required-visual-llm-files)
    - [Main window options](#main-window-options)
  - [Document analysis (Text LLM)](#document-analysis-text-llm)
    - [Supported document formats](#supported-document-formats)
    - [Main window options (documents)](#main-window-options-documents)
  - [Audio/video metadata filename suggestions](#audiovideo-metadata-filename-suggestions)
    - [Supported audio/video formats](#supported-audiovideo-formats)
  - [System compatibility check](#system-compatibility-check)
  - [Requirements](#requirements)
  - [Installation](#installation)
    - [Linux](#linux)
    - [macOS](#macos)
    - [Windows](#windows)
  - [Categorization cache database](#categorization-cache-database)
  - [Uninstallation](#uninstallation)
  - [Using your OpenAI API key](#using-your-openai-api-key)
  - [Using your Gemini API key](#using-your-gemini-api-key)
  - [Testing](#testing)
  - [Diagnostics](#diagnostics)
  - [How to Use](#how-to-use)
  - [Sorting a Remote Directory (e.g., NAS)](#sorting-a-remote-directory-eg-nas)
  - [Contributing](#contributing)
  - [Credits](#credits)
  - [License](#license)
  - [Donation](#donation)

---

## Changelog

## [1.8.0] - 2026-04-02

- New plugin system with install, update, and removal via a dedicated Plugins menu.
- Install plugins locally (.aifsplugin) or fetch them from an online catalog.
- New OneDrive plugin with improved sync handling, safer moves, and better undo support.

## [1.7.3] - 2026-03-22

- Non-English categorization is now more reliable: files are categorized canonically in English first, then translated into the selected category language. This change is due to LLM language limitations.
- App updates now support separate update streams for Windows, macOS, and Linux, while still accepting the legacy single-stream manifest format for newer clients.
- Windows feeds can now provide a direct installer URL plus SHA-256 checksum so the app can download the installer, show download progress, verify its integrity, and launch it after confirmation.
- The UI translation system was migrated fully to Qt `.ts` / `.qm` catalogs.
- Local categorization with local LLMs is now more robust.
- Cached category labels are sanitized more aggressively to avoid malformed UTF-8 data breaking later categorization or display.
- Misc improvements.
- Misc bug fixes.

See [CHANGELOG.md](CHANGELOG.md) for the full history.

---

## Features

- **AI-Powered Categorization**: Classify files intelligently using either a **local LLM** (Llama, Mistral) or a remote model (ChatGPT with your own OpenAI API key, or Gemini with your own Gemini API key).
- **Offline-Friendly**: Use a local LLM to categorize files entirely - no internet or API key required.
- **Robust categorization**: Taxonomy and heuristics help keep labels more consistent across runs.
- **Customizable sorting rules**: Automatically assign categories and subcategories for granular organization.
- **Two categorization modes**: Pick **More Refined** for detailed labels or **More Consistent** to bias toward uniform categories within a folder.
- **Category whitelists**: Define named whitelists of allowed categories/subcategories, manage them under **Settings → Manage category whitelists…**, and toggle/select them in the main window when you want to constrain model output for a session.
- **Multilingual categorization**: Have the LLM assign categories in Dutch, French, German, Italian, Polish, Portuguese, Spanish, or Turkish (model dependent).
- **Custom local LLMs**: Register your own local GGUF models directly from the **Select LLM** dialog.
- **Image content analysis (Visual LLM)**: Analyze supported picture files with LLaVA to produce descriptions and optional filename suggestions (rename-only mode supported).
- **Image date-to-category suffix (optional)**: Append image creation date metadata to image category names when available.
- **Document content analysis (Text LLM)**: Analyze supported document files to summarize content and suggest filenames; uses the same selected LLM (local or remote).
- **Audio/video metadata filename suggestions**: Turn embedded media tags into clean, library-style filenames for supported audio and video files, with full review before anything is renamed.
- **Sortable review**: Sort the Categorization Review table by file name, category, or subcategory to triage faster.
- **Qt6 Interface**: Lightweight and responsive UI with refreshed menus and icons.
- **Interface languages**: English, Dutch, French, German, Italian, Korean, Spanish, and Turkish.
- **Cross-Platform Compatibility**: Works on Windows, macOS, and Linux.
- **Local Database Caching**: Speeds up repeated categorization and minimizes remote LLM usage costs.
- **Sorting Preview**: See how files will be organized before confirming changes.
- **Dry run** / preview-only mode to inspect planned moves without touching files.
- **Persistent Undo** ("Undo last run") even after closing the sort dialog.
- **Bring your own key**: Paste your OpenAI or Gemini API key once; it's stored locally and reused for remote runs.
- **Update Notifications**: Get notified about updates - with optional or required update flows.
- **Storage plugin support**: Install provider-specific compatibility modes from the **Plugins** menu when the app detects supported cloud-backed folders.
- **Dedicated OneDrive support**: Use the OneDrive plugin for stronger sync-aware handling than plain local-folder mode.

---

## Categorization

### Categorization modes

- **More refined**: The flexible, detail-oriented mode. Consistency hints are disabled so the model can pick the most specific category/subcategory it deems appropriate, which is useful for long-tail or mixed folders.
- **More consistent**: The uniform mode. The model receives consistency hints from prior assignments in the current session so files with similar names/extensions trend toward the same categories. This is helpful when you want strict uniformity across a batch.
- Switch between the two via the **Categorization type** radio buttons on the main window; your choice is saved for the next run.

### Category whitelists

- Enable **Use a whitelist** to inject the selected whitelist into the LLM prompt; disable it to let the model choose freely.
- Manage lists (add, edit, remove) under **Settings → Manage category whitelists…**. Built-in `Default` and `Documents` lists are auto-created only when no lists exist, and multiple named lists can be kept for different projects.
- Keep each whitelist to roughly **15–20 categories/subcategories** to avoid overlong prompts on smaller local models. Use several narrower lists instead of a single very long one.
- Whitelists apply in either categorization mode; pair them with **More consistent** when you want the strongest adherence to a constrained vocabulary.

---

## Image analysis (Visual LLM)

Image analysis uses a local LLaVA-based visual LLM to describe image contents and (optionally) suggest a better filename. This runs locally and does not require an API key.

### Required visual LLM files

The **Select LLM** dialog now includes an "Image analysis models (LLaVA)" section with two downloads:

- **LLaVA text model (GGUF)**: The main language model that produces the description and the filename suggestion.
- **LLaVA mmproj (vision encoder projection, GGUF)**: The adapter that maps vision embeddings into the LLM token space so the model can accept images.

Both files are required. If either one is missing, image analysis is disabled and the app will prompt to open the **Select LLM** dialog to download them. The download URLs can be overridden with `LLAVA_MODEL_URL` and `LLAVA_MMPROJ_URL` (see [Environment variables](#environment-variables)).

### Main window options

Image analysis adds six related checkboxes to the main window:

- **Analyze picture files by content (can be slow)**: Runs the visual LLM on supported picture files and reports progress in the analysis dialog.
- **Process picture files only (ignore any other files)**: Restricts the run to supported picture files and disables the categorization controls while active.
- **Add image creation date (if available) to category name**: Appends `YYYY-MM-DD` from image metadata to the category label when available. Disabled when rename-only is enabled.
- **Add photo date and place to filename (if available)**: Adds metadata-based date/place prefixes to suggested image filenames when available.
- **Offer to rename picture files**: Shows a **Suggested filename** column in the Review dialog with the visual LLM proposal. You can edit it before confirming.
- **Do not categorize picture files (only rename)**: Skips text categorization for images and keeps them in place while applying (optional) renames.

The separate top-level checkbox **Add audio/video metadata to file name (if available)** controls metadata-based rename suggestions for supported audio/video files. See [Audio/video metadata filename suggestions](#audiovideo-metadata-filename-suggestions).

---

## Document analysis (Text LLM)

Document analysis uses the same selected LLM (local or remote) to extract text from supported document files, summarize content, and optionally suggest a better filename. No extra model downloads are required.

### Supported document formats

- Plain text: `.txt`, `.md`, `.rtf`, `.csv`, `.tsv`, `.json`, `.xml`, `.yml`/`.yaml`, `.ini`/`.cfg`/`.conf`, `.log`, `.html`/`.htm`, `.tex`, `.rst`
- PDF: `.pdf` (embedded PDFium by default; CLI fallback via `pdftotext` is available only if you explicitly configure `-DAI_FILE_SORTER_REQUIRE_EMBEDDED_PDF_BACKEND=OFF`)
- Office/OpenOffice: `.docx`, `.xlsx`, `.pptx`, `.odt`, `.ods`, `.odp` (embedded libzip+pugixml in bundled builds; CLI fallback uses `unzip` if you build without vendored libs)
- Legacy binary formats like `.doc`, `.xls`, `.ppt` are not currently supported.

Source builds: embedded extractors are used by default. If the vendored PDFium artifacts are missing for your target platform, CMake now fails loudly instead of silently disabling PDF content extraction. You can opt back into the old CLI fallback with `-DAI_FILE_SORTER_REQUIRE_EMBEDDED_PDF_BACKEND=OFF`.

### Main window options (documents)

- **Analyze document files by content**: Extracts document text and feeds it into the LLM for summary + rename suggestion.
- **Process document files only (ignore any other files)**: Restricts the run to supported document files and disables the categorization controls while active.
- **Offer to rename document files**: Shows a **Suggested filename** column in the Review dialog with the LLM proposal. You can edit it before confirming.
- **Do not categorize document files (only rename)**: Skips text categorization for documents and keeps them in place while applying (optional) renames.
- **Add document creation date (if available) to category name**: Appends `YYYY-MM` from metadata when available. Disabled when rename-only is enabled.

---

## Audio/video metadata filename suggestions

Let AI File Sorter turn embedded media tags into clean, consistent filenames for your music and video library. When enabled, the app reads supported metadata fields and builds a polished suggested name in the format `year_artist_album_title.ext`. As with all rename suggestions, nothing is changed until you review and confirm it.

### Supported audio/video formats

- Audio extensions: `.aac`, `.aif`, `.aiff`, `.alac`, `.ape`, `.flac`, `.m4a`, `.mp3`, `.ogg`, `.oga`, `.opus`, `.wav`, `.wma`
- Video extensions: `.3gp`, `.avi`, `.flv`, `.m4v`, `.mkv`, `.mov`, `.mp4`, `.mpeg`, `.mpg`, `.mts`, `.m2ts`, `.ts`, `.webm`, `.wmv`
- Built-in tag readers currently cover MP3 (`ID3v1`/`ID3v2`), FLAC (Vorbis comments), OGG/OGA/Opus (Vorbis comments), and MP4-family containers such as `.m4a`, `.mp4`, `.m4v`, `.mov`, and `.3gp` (MP4/MOV metadata atoms).
- When compiled with package-managed `MediaInfoLib`, the same rename flow can also use metadata exposed by MediaInfo for additional supported containers when available.

---

## System compatibility check

The **System compatibility check** runs a quick benchmark that estimates how well your system can handle:

- **Categorization** with the selected local LLMs
- **Document analysis** by content
- **Image analysis** (visual LLM)

You can launch it from the menu (**File → System compatibility check…**). It only runs if at least one local or visual LLM is downloaded, and it won’t auto-rerun if it's already been run.

What it does:

- Detects available CPU threads and GPU backends (e.g., Vulkan/CUDA)
- Times a small categorization and document-analysis workload per default model
- Times a single image-analysis pass if visual LLM files are present
- Reports speed tiers (optimal / acceptable / a bit long) and suggests a recommended local LLM

Tip: quit CPU/GPU‑intensive apps before running the check for more accurate results.

---

## Requirements

- **Operating System**: Linux, macOS, or Windows. Linux/macOS source builds use the Makefile flow below; Windows source builds use the native Qt/MSVC + CMake flow in the Windows section.
- **Compiler**: A C++20-capable compiler (`g++` or `clang++` on Linux/macOS, MSVC 2022 on Windows).
- **Qt 6**: Core, Gui, Widgets modules and the Qt resource compiler (`qt6-base-dev` / `qt6-tools` on Linux, `brew install qt` on macOS, or a Qt 6 MSVC kit / `qtbase` via vcpkg on Windows).
- **Libraries**: `curl`, `sqlite3`, `fmt`, `spdlog`, `libmediainfo` (required for full source builds), and the prebuilt `llama` libraries shipped under `app/lib/precompiled` on Linux/Windows or `app/lib/precompiled-*` for macOS variant builds. On Windows, these non-Qt libraries are supplied through the `app/vcpkg.json` manifest.
- **MediaInfo policy**: MediaInfo must be installed through a package manager (`apt`/`dnf`/`pacman`/`brew`/`vcpkg`). The build rejects vendored MediaInfo submodules and checked-in binaries.
- **Document analysis libraries** (vendored): PDFium, libzip, and pugixml. PDFium is required by default so packaged/source builds keep PDF extraction embedded on Windows, macOS, and Linux; set `-DAI_FILE_SORTER_REQUIRE_EMBEDDED_PDF_BACKEND=OFF` only if you intentionally want the `pdftotext` fallback.
- **Optional GPU backends**: A Vulkan 1.2+ runtime (preferred) or CUDA 12.x for NVIDIA cards. `StartAiFileSorter.exe`/`run_aifilesorter.sh` auto-detect the best available backend and fall back to CPU/OpenBLAS automatically, so CUDA is never required to run the app.
- **Git** (optional): For cloning this repository. Archives can also be downloaded.
- **OpenAI or Gemini API key** (optional): Required only when using the remote ChatGPT or Gemini workflow.

---

## Installation

File categorization with local LLMs is completely free of charge. If you prefer to use a remote workflow (ChatGPT or Gemini) you will need your own API key with a small balance or within the free tier (see [Using your OpenAI API key](#using-your-openai-api-key) or [Using your Gemini API key](#using-your-gemini-api-key)).

### Linux

#### Prebuilt Debian/Ubuntu package

1. **Install runtime prerequisites** (Qt6, networking, database, math libraries):
   - Ubuntu 24.04 / Debian 12:
     ```bash
     sudo apt update && sudo apt install -y \
       libqt6widgets6 libcurl4 libjsoncpp25 libfmt9 libopenblas0-pthread \
       libvulkan1 mesa-vulkan-drivers patchelf
     ```
   - Debian 13 (trixie):
     ```bash
     sudo apt update && sudo apt install -y \
       libqt6widgets6 libcurl4t64 libjsoncpp26 libfmt10 libopenblas0-pthread \
       libvulkan1 mesa-vulkan-drivers patchelf
     ```
   If you build the Vulkan backend from source, install `glslc` (Debian/Ubuntu package: `glslc`; on some distros: `shaderc` or `shaderc-tools`).
   On Debian 13, use `libjsoncpp26`, `libfmt10`, and `libcurl4t64` (APT may auto-select `libcurl4t64` if `libcurl4` is not available).
   Ensure that the Qt platform plugins are installed (on Ubuntu 22.04 this is provided by `qt6-wayland`).
   GPU acceleration additionally requires either a working Vulkan 1.2+ stack (Mesa, AMD/Intel/NVIDIA drivers) or, for NVIDIA users, the matching CUDA runtime (`nvidia-cuda-toolkit` or vendor packages). The launcher automatically prefers Vulkan when both are present and falls back to CPU if neither is available.
2. **Install the package**
   ```bash
   sudo apt install ./aifilesorter_*.deb
   ```
   Using `apt install` (rather than `dpkg -i`) ensures any missing dependencies listed above are installed automatically.

#### Build from source

1. **Install dependencies**
   - Debian / Ubuntu:
    ```bash
    sudo apt update && sudo apt install -y \
      build-essential cmake git qt6-base-dev qt6-base-dev-tools qt6-l10n-tools qt6-tools-dev-tools \
      libcurl4-openssl-dev libjsoncpp-dev libsqlite3-dev libssl-dev libfmt-dev libspdlog-dev libmediainfo-dev \
      zlib1g-dev
    ```
   - Fedora / RHEL:

    ```bash
    export PATH="/usr/lib64/qt6/libexec:$PATH"
    sudo dnf install -y gcc-c++ cmake git qt6-qtbase-devel qt6-qttools-devel \
      libcurl-devel jsoncpp-devel sqlite-devel openssl-devel fmt-devel spdlog-devel mediainfo-devel
    ```

   - Arch / Manjaro:

    ```bash
     sudo pacman -S --needed base-devel git cmake qt6-base qt6-tools curl jsoncpp sqlite openssl fmt spdlog mediainfo
    ```

     Optional GPU acceleration also requires either the distro Vulkan 1.2+ driver/runtime (Mesa, AMD, Intel, NVIDIA) or CUDA packages for NVIDIA cards. Install whichever stack you plan to use; the app will fall back to CPU automatically if none are detected.
     MediaInfo is enforced as package-managed only; vendored `MediaInfoLib` folders or repo-local binaries are rejected by the build.

2. **Clone the repository**

   ```bash
   git clone https://github.com/hyperfield/ai-file-sorter.git
   cd ai-file-sorter
   git submodule update --init --recursive
   ```

   > **Submodule tip:** If you previously downloaded `llama.cpp` or Catch2 manually, remove or rename `app/include/external/llama.cpp` and `external/Catch2` before running the `git submodule` command. Git needs those directories to be empty so it can populate them with the tracked submodules.

3. **Build vendored libzip** (generates `zipconf.h` and `libzip.a`)

   ```bash
   cmake -S external/libzip -B external/libzip/build \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_DOC=OFF \
    -DENABLE_BZIP2=OFF \
    -DENABLE_LZMA=OFF \
    -DENABLE_ZSTD=OFF \
    -DENABLE_OPENSSL=OFF \
    -DENABLE_GNUTLS=OFF \
    -DENABLE_MBEDTLS=OFF \
    -DENABLE_COMMONCRYPTO=OFF \
    -DENABLE_WINDOWS_CRYPTO=OFF

   cmake --build external/libzip/build
   ```

   On Ubuntu/Debian you will also need the Zlib development headers (`zlib1g-dev`) or
   the libzip configure step will fail.

   If you prefer system headers instead, install `libzip-dev` and ensure `zipconf.h` is on your include path.

4. **Build the llama runtime variants** (run once per backend you plan to ship/test)

   ```bash
   # CPU / OpenBLAS
   ./app/scripts/build_llama_linux.sh cuda=off vulkan=off
   # CUDA (optional; requires NVIDIA driver + CUDA toolkit)
   ./app/scripts/build_llama_linux.sh cuda=on vulkan=off
   # Vulkan (optional; requires a working Vulkan 1.2+ stack and glslc, e.g. mesa-vulkan-drivers + vulkan-tools + glslc)
   ./app/scripts/build_llama_linux.sh cuda=off vulkan=on
   ```

   Each invocation stages the corresponding `llama`/`ggml` libraries under `app/lib/precompiled/<variant>` and the runtime DLL/SO copies under `app/lib/ggml/w<variant>`. The script refuses to enable CUDA and Vulkan simultaneously, so run it separately for each backend. Shipping both directories lets the launcher pick Vulkan when available, then CUDA, and otherwise stay on CPU—no CUDA-only dependency remains.

5. **Compile the application**

   ```bash
   cd app
   make -j4
   ```

   The binary is produced at `app/bin/aifilesorter`.
   The Makefile requires `pkg-config` + package-managed `libmediainfo`; it intentionally rejects vendored MediaInfo copies.

6. **Install system-wide (optional)**

   ```bash
   sudo make install
   ```

7. **Build a Debian package (optional)**

   ```bash
   ./app/scripts/package_deb.sh
   ```

   The packaging script always bundles the CPU runtime and auto-includes any staged GPU
   variants already present under `app/lib/precompiled` (for example `vulkan` after
   `./app/scripts/build_llama_linux.sh cuda=off vulkan=on`). Use
   `./app/scripts/package_deb.sh --cpu-only` for a smaller CPU-only package, or
   `--include-vulkan` / `--include-cuda` if you want the script to fail when a specific
   staged variant is missing.

### macOS

1. **Install Xcode command-line tools** (`xcode-select --install`).
2. **Install Homebrew** (if required).
3. **Install dependencies**

   ```bash
   brew install qt curl jsoncpp sqlite openssl fmt spdlog mediainfo cmake git pkgconfig libffi
   ```

   Add Qt to your environment if it is not already present:

   ```bash
   export PATH="$(brew --prefix)/opt/qt/bin:$PATH"
   export PKG_CONFIG_PATH="$(brew --prefix)/lib/pkgconfig:$(brew --prefix)/share/pkgconfig:$PKG_CONFIG_PATH"
   ```

4. **Clone the repository and submodules** (same commands as Linux).
   > The macOS build pins `MACOSX_DEPLOYMENT_TARGET=11.0` so the Mach-O `LC_BUILD_VERSION` covers Apple Silicon and newer releases (including Sequoia). Raise or lower it (e.g., `export MACOSX_DEPLOYMENT_TARGET=15.0`) if you need a different floor.

5. **Build vendored libzip** (generates `zipconf.h` and `libzip.a`)

   ```bash
   cmake -S external/libzip -B external/libzip/build \
     -DBUILD_SHARED_LIBS=OFF \
     -DBUILD_DOC=OFF \
     -DENABLE_BZIP2=OFF \
     -DENABLE_LZMA=OFF \
     -DENABLE_ZSTD=OFF \
     -DENABLE_OPENSSL=OFF \
     -DENABLE_GNUTLS=OFF \
     -DENABLE_MBEDTLS=OFF \
     -DENABLE_COMMONCRYPTO=OFF \
     -DENABLE_WINDOWS_CRYPTO=OFF
   cmake --build external/libzip/build
   ```

6. **Build the llama runtime**

   ```bash
   ./app/scripts/build_llama_macos.sh
   ```
   Architecture-specific examples:

   ```bash
   ./app/scripts/build_llama_macos.sh --arm64   # Apple Silicon
   ./app/scripts/build_llama_macos.sh --intel   # Intel Mac
   ```
   The macOS app and `.app` bundles use the runtime staged under `app/lib/precompiled*`; they do not need Homebrew `ggml` or `llama.cpp` libraries.
   If you have older `ggml` / `llama.cpp` copies installed in generic library locations, prefer unlinking or removing them instead of relying on them implicitly.
7. **Compile the application**

   ```bash
   cd app
   make -j8                 # use -jN to control parallelism
   sudo make install   # optional
   ```

   The default build places the binary at `app/bin/aifilesorter`.

   **Variant targets:**

   ```bash
   make -j8 MACOS_LLAMA_M1    # outputs app/bin/m1/aifilesorter
   make -j8 MACOS_LLAMA_M2    # outputs app/bin/m2/aifilesorter
   make -j8 MACOS_LLAMA_INTEL # outputs app/bin/intel/aifilesorter
   ```

   These targets rebuild the llama.cpp runtime before compiling the app.
   On a native Intel Mac, the most direct path is:

   ```bash
   cd app
   make -j8 MACOS_LLAMA_INTEL
   ```

   That target assumes the normal Intel Homebrew prefix (`/usr/local`) and produces `app/bin/intel/aifilesorter`.
   When cross-compiling Intel on Apple Silicon, use x86_64 Homebrew (under `/usr/local`) or set `BREW_PREFIX=/usr/local` so Qt/pkg-config resolve correctly.
   `sudo make install` places the macOS runtime libraries under `/usr/local/lib/aifilesorter` to avoid collisions with unrelated system or Homebrew ggml libraries.
   The commands above build the raw executable only; they do **not** currently create a distributable `.app` bundle or `.dmg`.
   This repository does not yet ship a documented or automated macOS bundle/DMG packaging target in `README.md`, so any `.app` / `.dmg` release packaging must be handled as a separate macOS-hosted release step.
   Each variant uses distinct build directories to avoid cross-arch collisions:
   - llama.cpp libs: `app/lib/precompiled-m1`, `app/lib/precompiled-m2`, `app/lib/precompiled-intel`
   - object files: `app/obj/arm64` or `app/obj/x86_64`

### Windows

Build now targets native MSVC + Qt6 without MSYS2. Two options are supported; the vcpkg route is simplest.

Option A - CMake + vcpkg (recommended)

1. Install prerequisites:
   - Visual Studio 2022 with Desktop C++ workload
   - CMake 3.21+ (Visual Studio ships a recent version)
   - vcpkg: <https://github.com/microsoft/vcpkg> (clone and bootstrap)
   - package-managed `libmediainfo` via vcpkg manifest (no vendored MediaInfo submodule/binaries)
   - **MSYS2 MinGW64 + OpenBLAS**: install MSYS2 from <https://www.msys2.org>, open an *MSYS2 MINGW64* shell, and run `pacman -S --needed mingw-w64-x86_64-openblas`. The `build_llama_windows.ps1` script uses this OpenBLAS copy for CPU-only builds (the vcpkg variant is not suitable), defaulting to `C:\msys64\mingw64` unless you pass `openblasroot=<path>` or set `OPENBLAS_ROOT`.
2. Clone repo and submodules:

   ```powershell
   git clone https://github.com/hyperfield/ai-file-sorter.git
   cd ai-file-sorter
   git submodule update --init --recursive
   ```

3. **Build vendored libzip** (generates `zipconf.h` and `libzip.lib`)

   Run from the same x64 Native Tools / VS Developer PowerShell you will use to build the app:

   ```powershell
   cmake -S external\libzip -B external\libzip\build -A x64 `
     -DBUILD_SHARED_LIBS=OFF `
     -DBUILD_DOC=OFF `
     -DENABLE_BZIP2=OFF `
     -DENABLE_LZMA=OFF `
     -DENABLE_ZSTD=OFF `
     -DENABLE_OPENSSL=OFF `
     -DENABLE_GNUTLS=OFF `
     -DENABLE_MBEDTLS=OFF `
     -DENABLE_COMMONCRYPTO=OFF `
     -DENABLE_WINDOWS_CRYPTO=OFF
   cmake --build external\libzip\build --config Release
   ```

4. Determine your vcpkg root. It is the folder that contains `vcpkg.exe` (for example `C:\dev\vcpkg`).
    - If `vcpkg` is on your `PATH`, run this command to print the location:

      ```powershell
      Split-Path -Parent (Get-Command vcpkg).Source
      ```

    - Otherwise use the directory where you cloned vcpkg.

   MediaInfo note: you do **not** manually add `MediaInfoLib` include/lib paths on Windows. The project already declares `libmediainfo` in `app/vcpkg.json`, and `app\build_windows.ps1` configures CMake with the vcpkg toolchain + manifest so `find_package(MediaInfoLib ...)` resolves it automatically. If you want to preinstall or verify it explicitly, run `vcpkg install libmediainfo:x64-windows`.
5. Build the bundled `llama.cpp` runtime variants (run from the same **x64 Native Tools** / **VS 2022 Developer PowerShell** shell). Invoke the script once per backend you need. Make sure the MSYS2 OpenBLAS install from step 1 is present before running the CPU-only variant (or pass `openblasroot=<path>` explicitly):

   ```powershell
   # CPU / OpenBLAS only
   app\scripts\build_llama_windows.ps1 cuda=off vulkan=off vcpkgroot=C:\dev\vcpkg
   # CUDA (requires matching NVIDIA toolkit/driver)
   app\scripts\build_llama_windows.ps1 cuda=on vulkan=off vcpkgroot=C:\dev\vcpkg
   # Vulkan (requires LunarG Vulkan SDK or vendor Vulkan 1.2+ runtime)
   app\scripts\build_llama_windows.ps1 cuda=off vulkan=on vcpkgroot=C:\dev\vcpkg
   ```
  
  Each run emits the appropriate `llama.dll` / `ggml*.dll` pair under `app\lib\precompiled\<cpu|cuda|vulkan>` and copies the runtime DLLs into `app\lib\ggml\w<variant>`. For Vulkan builds, install the latest LunarG Vulkan SDK (or the vendor's runtime), ensure `vulkaninfo` succeeds in the same shell, and then run the script. Supplying both Vulkan and (optionally) CUDA artifacts lets `StartAiFileSorter.exe` detect the best backend at launch—Vulkan is preferred, CUDA is used when Vulkan is missing, and CPU remains the fallback, so CUDA is not required.

6. Build the Qt6 application using the helper script (still in the VS shell). The helper stages runtime DLLs via `windeployqt`, shares one dependency install tree across variants, and by default produces three Windows builds in one run:

   ```powershell
   # One-time per shell if script execution is blocked:
   Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass

   app\build_windows.ps1 -Configuration Release -VcpkgRoot C:\dev\vcpkg
   ```

   - Replace `C:\dev\vcpkg` with the path where you cloned vcpkg; it must contain `scripts\buildsystems\vcpkg.cmake`.
   - The helper produces these output directories by default:
     - Standard installer build with Windows auto-update enabled: `app\build-windows\Release`
     - Microsoft Store build with update checks disabled: `app\build-windows-store\Release`
     - Standalone Windows build with notification-only/manual updates: `app\build-windows-standalone\Release`
   - Use `-Variants Standard`, `-Variants MsStore`, or `-Variants Standalone` to build only a subset.
   - `aifilesorter.exe` is the primary Windows GUI entry point. `StartAiFileSorter.exe` is still built beside it as the legacy bootstrapper and carries the same updater mode.
   - `-VcpkgRoot` is optional if `VCPKG_ROOT`/`VPKG_ROOT` is set or `vcpkg`/`vpkg` is on `PATH`.
   - Each variant directory receives its own executable and staged Qt/third-party DLLs. Pass `-SkipDeploy` if you only want the binaries without bundling runtime DLLs.
   - Pass `-Parallel <N>` to override the default “all cores” parallel build behaviour (for example, `-Parallel 8`). By default the script invokes `cmake --build ... --parallel <core-count>` and `ctest -j <core-count>` to keep both MSBuild and Ninja fully utilized.

Option B - CMake + Qt online installer

1. Install prerequisites:
   - Visual Studio 2022 with Desktop C++ workload
   - Qt 6.x MSVC kit via Qt Online Installer (e.g., Qt 6.6+ with MSVC 2019/2022)
   - CMake 3.21+
   - vcpkg (for non-Qt libs): curl, jsoncpp, sqlite3, openssl, fmt, spdlog, gettext, libmediainfo
2. **Build vendored libzip** (generates `zipconf.h` and `libzip.lib`)

   Run from the same x64 Native Tools / VS Developer PowerShell you will use to build the app:

   ```powershell
   cmake -S external\libzip -B external\libzip\build -A x64 `
     -DBUILD_SHARED_LIBS=OFF `
     -DBUILD_DOC=OFF `
     -DENABLE_BZIP2=OFF `
     -DENABLE_LZMA=OFF `
     -DENABLE_ZSTD=OFF `
     -DENABLE_OPENSSL=OFF `
     -DENABLE_GNUTLS=OFF `
     -DENABLE_MBEDTLS=OFF `
     -DENABLE_COMMONCRYPTO=OFF `
     -DENABLE_WINDOWS_CRYPTO=OFF
   cmake --build external\libzip\build --config Release
   ```

3. Build the bundled `llama.cpp` runtime (same VS shell). Any missing OpenBLAS/cURL packages are installed automatically via vcpkg:

   ```powershell
   pwsh .\app\scripts\build_llama_windows.ps1 [cuda=on|off] [vulkan=on|off] [vcpkgroot=C:\dev\vcpkg]
   ```

   This is required before configuring the GUI because the build links against the produced `llama` static libraries/DLLs.
4. Configure CMake from the repo root so CMake sees both the Qt install and the app's vcpkg manifest (adapt `CMAKE_PREFIX_PATH` to your Qt install):

    ```powershell
    $env:VCPKG_ROOT = "C:\path\to\vcpkg"  # e.g. C:\dev\vcpkg
    $qt = "C:\Qt\6.6.3\msvc2019_64"  # example
    cmake -S app -B build -G "Ninja" `
      -DCMAKE_PREFIX_PATH=$qt `
     -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake `
     -DVCPKG_MANIFEST_DIR=app `
     -DAI_FILE_SORTER_REQUIRE_MEDIAINFOLIB=ON `
     -DVCPKG_TARGET_TRIPLET=x64-windows
   cmake --build build --config Release
   ```

   This configure step enables vcpkg manifest mode, so `libmediainfo` is installed/resolved from `app\vcpkg.json` automatically. No manual linker or include-path edits are needed for MediaInfo on Windows.

Notes

- To rebuild from scratch, run `.\app\build_windows.ps1 -Clean`. The script removes the selected variant build directories and the shared `app\build-windows-vcpkg_installed` dependency tree before configuring.
- Runtime DLLs are copied automatically via `windeployqt` after each successful build; skip this step with `-SkipDeploy` if you manage deployment yourself.
- If Visual Studio sets `VCPKG_ROOT` to its bundled copy under `Program Files`, clone vcpkg to a writable directory (for example `C:\dev\vcpkg`) and pass `vcpkgroot=<path>` when running `build_llama_windows.ps1`.
- If you plan to ship CUDA or Vulkan acceleration, run the `build_llama_*` helper for each backend you intend to include before configuring CMake so the libraries exist. The runtime can carry both and auto-select at launch, so CUDA remains optional.
- `-BuildTests` and `-RunTests` currently build and execute tests only in the `Standard` variant, which is the primary Windows development/CI configuration.

### Running tests

Catch2-based unit tests are optional. Enable them via CMake:

```bash
cmake -S app -B build-tests -DAI_FILE_SORTER_BUILD_TESTS=ON -DAI_FILE_SORTER_REQUIRE_MEDIAINFOLIB=ON
cmake --build build-tests --target ai_file_sorter_tests --parallel $(nproc)
ctest --test-dir build-tests --output-on-failure -j $(nproc)
```

On macOS, replace `$(nproc)` with `$(sysctl -n hw.ncpu)`.

On Windows (PowerShell), use:

```powershell
cmake -S app -B build-tests -DAI_FILE_SORTER_BUILD_TESTS=ON -DAI_FILE_SORTER_REQUIRE_MEDIAINFOLIB=ON
cmake --build build-tests --target ai_file_sorter_tests --parallel $env:NUMBER_OF_PROCESSORS
ctest --test-dir build-tests --output-on-failure -j $env:NUMBER_OF_PROCESSORS
```

Notes

- List individual Catch2 cases: `./build-tests/ai_file_sorter_tests --list-tests`
- Print each case name (including successes): `./build-tests/ai_file_sorter_tests --verbosity high --success`

On Windows you can pass `-BuildTests` (and `-RunTests` to execute `ctest`) to `app\build_windows.ps1`:

```powershell
app\build_windows.ps1 -Configuration Release -Variants Standard -BuildTests -RunTests
```

The current suite (under `tests/unit`) focuses on core utilities; expand it as new functionality gains coverage.

### Selecting a backend at runtime

Both the Linux launcher (`app/bin/run_aifilesorter.sh` / `aifilesorter-bin`) and the Windows starter accept the following optional flags:

- `--cuda={on|off}` – force-enable or disable the CUDA backend.
- `--vulkan={on|off}` – force-enable or disable the Vulkan backend.

When no flags are provided the app auto-detects available runtimes in priority order (Vulkan → CUDA → CPU). Use the flags to skip a backend (`--cuda=off` forces Vulkan/CPU even if CUDA is installed, `--vulkan=off` tests CUDA explicitly) or to validate a newly installed stack (`--vulkan=on`). Passing `on` to both flags is rejected, and if neither GPU backend is detected the app automatically stays on CPU.

#### Vulkan and VRAM notes

- Vulkan is preferred when available; CUDA is used only if Vulkan is missing or explicitly requested.
- The app auto-estimates `n_gpu_layers` based on available VRAM. Integrated GPUs are capped to 4 GiB for safety, which can limit offloading.
- If VRAM is tight, the app may fall back to CPU or reduce offload. As a rule of thumb, 8 GB+ VRAM provides a smoother experience for Vulkan offload and image analysis; 4 GB often results in partial offload or CPU fallback.
- Override auto-estimation with `AI_FILE_SORTER_N_GPU_LAYERS` (`-1` auto, `0` force CPU) or `AI_FILE_SORTER_GPU_BACKEND=cpu`.
- For image analysis, `AI_FILE_SORTER_VISUAL_USE_GPU=0` forces the visual encoder to run on CPU to avoid VRAM allocation errors.

### Environment variables

Runtime and GPU:

- `AI_FILE_SORTER_GPU_BACKEND` - select GPU backend: `auto` (default), `vulkan`, `cuda`, or `cpu`.
- `AI_FILE_SORTER_N_GPU_LAYERS` - override `n_gpu_layers` for llama.cpp; `-1` = auto, `0` = force CPU.
- `AI_FILE_SORTER_CTX_TOKENS` - override local LLM context length (default 2048; clamped 512-8192).
- `AI_FILE_SORTER_GGML_DIR` - directory to load ggml backend shared libraries from. On macOS this is only auto-discovered from bundled or sibling app runtime directories; use this variable explicitly if you want a custom ggml runtime.

Visual LLM:

- `LLAVA_MODEL_URL` - download URL for the visual LLM GGUF model (required to enable image analysis).
- `LLAVA_MMPROJ_URL` - download URL for the visual LLM mmproj GGUF file (required to enable image analysis).
- `AI_FILE_SORTER_VISUAL_USE_GPU` - force visual encoder GPU usage (`1`) or CPU (`0`). Defaults to auto; Vulkan may fall back to CPU if VRAM is low.

Timeouts and logging:

- `AI_FILE_SORTER_LOCAL_LLM_TIMEOUT` - seconds to wait for local LLM responses (default 60).
- `AI_FILE_SORTER_REMOTE_LLM_TIMEOUT` - seconds to wait for OpenAI/Gemini responses (default 10).
- `AI_FILE_SORTER_CUSTOM_LLM_TIMEOUT` - seconds to wait for custom OpenAI-compatible API responses (default 60).
- `AI_FILE_SORTER_LLAMA_LOGS` - enable verbose llama.cpp logs (`1`/`true`); also honors `LLAMA_CPP_DEBUG_LOGS`.

Storage and updates:

- `AI_FILE_SORTER_CONFIG_DIR` - override the base config directory (where `config.ini` lives).
- `CATEGORIZATION_CACHE_FILE` - override the SQLite cache filename inside the config dir.
- `UPDATE_SPEC_FILE_URL` - override the update feed spec URL (dev/testing). The updater now reads per-platform streams from `update.windows`, `update.macos`, and `update.linux`, with legacy single-stream feeds still accepted.
- `AI_FILE_SORTER_UPDATER_TEST_MODE` - enable Windows updater live-test mode (`1`/`true`). When enabled, the app skips the update feed fetch and synthesizes a newer version from the values below.
- `AI_FILE_SORTER_UPDATER_TEST_URL` - direct URL for the Windows updater live-test package. This can point to an `.exe`, `.msi`, or a `.zip` containing exactly one `.exe` or `.msi`.
- `AI_FILE_SORTER_UPDATER_TEST_SHA256` - SHA-256 checksum for the downloaded live-test package. If the URL points to a ZIP, this checksum must be for the ZIP archive itself.
- `AI_FILE_SORTER_UPDATER_TEST_VERSION` - optional synthetic version shown by live-test mode. Defaults to the current app version with an extra trailing segment, for example `1.7.2.1`.
- `AI_FILE_SORTER_UPDATER_TEST_MIN_VERSION` - optional synthetic minimum version for live-test mode. Defaults to `0.0.0` so the test behaves like an optional update.

Example update feed:

```json
{
  "update": {
    "current_version": "1.7.1",
    "min_version": "1.6.0",
    "download_url": "https://filesorter.app/download",
    "windows": {
      "current_version": "1.7.1",
      "min_version": "1.6.0",
      "download_url": "https://filesorter.app/download",
      "installer_url": "https://filesorter.app/downloads/AIFileSorterSetup-1.7.1.exe",
      "installer_sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    },
    "macos": {
      "current_version": "1.7.1",
      "min_version": "1.6.0",
      "download_url": "https://filesorter.app/download"
    },
    "linux": {
      "current_version": "1.7.1",
      "min_version": "1.6.0",
      "download_url": "https://filesorter.app/download"
    }
  }
}
```

Compatibility note:

- Older app versions only read the flat top-level fields under `update`, so keep `current_version`, `min_version`, and `download_url` there as a legacy compatibility stream if you still need to support them.
- Newer app versions prefer the platform-specific streams and will use `update.windows`, `update.macos`, or `update.linux` when present.
- The legacy compatibility stream can only represent one generic stream, not separate per-platform versions or installers.

Windows-only direct installer updates:

- `installer_url` - direct URL to the Windows installer package.
- `installer_sha256` - SHA-256 checksum used to verify the downloaded installer before launch.
- `installer_url` can now also point to a ZIP archive, as long as the archive contains exactly one installer payload (`.exe` or `.msi`).
- When both fields are present on Windows, the app can download the installer, verify it, and then prompt: `Quit the app and launch the installer to update`.

Windows updater live-test mode:

- `aifilesorter.exe` accepts the following flags directly on Windows:
  `--updater-live-test`
  `--updater-live-test-url=<https://.../AIFileSorterSetup.zip>`
  `--updater-live-test-sha256=<sha256-of-the-downloaded-package>`
  `--updater-live-test-version=<optional-version>`
  `--updater-live-test-min-version=<optional-min-version>`
- `StartAiFileSorter.exe` accepts and forwards the same flag family if you still use the bootstrapper path.
- Live-test mode is Windows-only and intentionally bypasses the normal update JSON feed.
- If the ZIP contains more than one `.exe` or `.msi`, the updater stops instead of guessing which installer to launch.
- If `--updater-live-test` is present and the URL / SHA flags are omitted, `aifilesorter.exe` also looks for a `live-test.ini` file next to the executable and fills in the missing values from there.
- Command-line flags still win over `live-test.ini`, so you can keep a default file and override just one field when needed.

Example `live-test.ini`:

```ini
[LiveTest]
download_url = https://files.example.com/AIFileSorterSetup-1.7.3.zip
sha256 = 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
current_version = 1.7.3
min_version = 0.0.0
```

Example PowerShell launch:

```powershell
.\aifilesorter.exe `
  --development `
  --updater-live-test
```

---

## Categorization cache database

AI File Sorter stores categorization results in a local SQLite database next to `config.ini` (the base directory can be overridden via `AI_FILE_SORTER_CONFIG_DIR`). This cache allows the app to skip already-processed files and preserve rename suggestions between runs.

What is stored:

- Directory path, file name, and file type (used as a unique key).
- Category/subcategory, taxonomy id, categorization style, and timestamp.
- Suggested filename (for picture and document rename suggestions).
- Rename-only flag (used when picture/document rename-only modes are enabled).
- Rename-applied flag (marks when a rename was executed so it is not offered again).

If you rename or move a file from the Review dialog, the cache entry is updated to the new name. Already-renamed picture files are skipped for visual analysis and rename suggestions on later runs. In the Review dialog, those already-renamed rows are hidden when rename-only is enabled, but they stay visible when categorization is enabled so you can still move them into category folders. To reset a folder's cache, accept the recategorization prompt or delete the cache file (or point `CATEGORIZATION_CACHE_FILE` to a new filename).

---

## Uninstallation

- **Debian/Ubuntu package installs**: `sudo apt remove aifilesorter`
- **Linux source installs**: `cd app && sudo make uninstall`
- **macOS source installs**: `cd app && sudo make uninstall`

For source installs, `make uninstall` removes the executable and the staged precompiled libraries. You can also delete cached local LLM models in `~/.local/share/aifilesorter/llms` (Linux) or `~/Library/Application Support/aifilesorter/llms` (macOS) if you no longer need them.

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

## Diagnostics

If you need to report a bug or collect troubleshooting data, use the bundled diagnostics scripts:

- **macOS:** `./app/scripts/collect_macos_diagnostics.sh`
- **Linux:** `./app/scripts/collect_linux_diagnostics.sh`
- **Windows (PowerShell):** `.\app\scripts\collect_windows_diagnostics.ps1`

Each script collects relevant logs, redacts common sensitive paths, and packages the result into a zip archive for sharing. See [app/scripts/README.md](app/scripts/README.md) for options such as time filtering and opening the output folder automatically.

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
- Llama: <https://www.llama.com>
- libzip: <https://libzip.org>
- Local File Organizer <https://github.com/QiuYannnn/Local-File-Organizer>
- llama.cpp <https://github.com/ggml-org/llama.cpp>
- MediaInfoLib: <https://mediaarea.net/en/MediaInfo>
- Mistral AI: <https://mistral.ai>
- OpenAI: <https://platform.openai.com/docs/overview>
- OpenSSL: <https://github.com/openssl/openssl>
- PDFium: <https://pdfium.googlesource.com/pdfium/>
- Poppler (pdftotext): <https://poppler.freedesktop.org/>
- pugixml: <https://pugixml.org>
- Qt: <https://www.qt.io/>
- spdlog: <https://github.com/gabime/spdlog>
- unzip (Info-ZIP): <https://infozip.sourceforge.net/>

## License

This project is licensed under the GNU AFFERO GENERAL PUBLIC LICENSE (GNU AGPL). See the [LICENSE](LICENSE) file for details, or https://www.gnu.org/licenses/agpl-3.0.html.

---

## Donation

Support the development of **AI File Sorter** and its future features. Every contribution counts!

- **[Donate](https://filesorter.app/donate/)**

---
