<!-- markdownlint-disable MD046 -->
# AI File Sorter

[![Code Version](https://img.shields.io/badge/Code-1.9.0-blue)](#)
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

The app can analyze picture files locally with built-in visual LLM backends and suggest meaningful, human-readable names. For example, a generic file like IMG_2048.jpg can be renamed to something descriptive such as clouds_over_lake.jpg. It can also analyze supported document files and propose clearer names based on their text content. AI File Sorter can also clean up messy audio and video filenames by using the metadata already stored inside supported media files. If tags such as year, artist, album, or title are available, the app can turn them into a clear suggestion like `2024_artist_album_title.mp3`, which you can review, edit, or ignore before any change is applied.

AI File Sorter helps tidy up cluttered folders such as Downloads, external drives, or NAS storage by grouping files based on their names, file types, folder context, and past sorting results.

Instead of relying only on fixed rules, the app combines AI suggestions with optional whitelists, recent similar results, and your approved review decisions. This helps keep sorting more consistent over time while still letting you review and adjust everything before anything is changed.

Categories (and optional subcategories) are suggested for each file, and for supported file types, rename suggestions are provided as well. Once you confirm, the required folders are created automatically and files are sorted accordingly.

Privacy-first by design:
AI File Sorter can run entirely on your device. When you use a local model, your files, filenames, images, and metadata stay on your computer, and no telemetry is sent. An internet connection is only needed if you choose to use a remote model.

---

#### How It Works

1. Point the app at a folder or drive  
2. Files (and image content, when applicable) are analyzed using the selected local or remote model  
3. Category and rename suggestions are generated  
4. You review and adjust if needed before anything is changed  

---

## Safe First Run

If you are trying AI File Sorter for the first time, start with a small test folder instead of a full archive or drive. Copy 20-50 files from `Downloads`, screenshots, photos, or documents into a temporary folder, run the analysis, and inspect the review table before applying anything.

This keeps the first run low risk: your files stay on your computer when you use local models, the AI only suggests categories and filenames, and no move or rename happens until you approve it. If you do apply changes and then want to reverse them, use **Edit -> Undo last run**.

---

[![Download ai-file-sorter](https://a.fsdn.com/con/app/sf-download-button)](https://sourceforge.net/projects/ai-file-sorter/files/latest/download)

[![Get it from Microsoft](https://get.microsoft.com/images/en-us%20dark.svg)](https://apps.microsoft.com/detail/9npk4dzd6r6s)

![AI File Sorter Screenshot](images/screenshots/ai-file-sorter-win.gif) ![AI File Sorter Screenshot](images/screenshots/main_windows_macos.png) ![AI File Sorter Screenshot](images/screenshots/sort-confirm-moved-win.png)

---

- [AI File Sorter](#ai-file-sorter)
  - [Safe First Run](#safe-first-run)
  - [Technical reference](#technical-reference)
  - [Changelog](#changelog)
  - [Features](#features)
  - [Categorization](#categorization)
    - [Categorization modes](#categorization-modes)
    - [Category language selection](#category-language-selection)
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
  - [Categorization cache and learned behavior](#categorization-cache-and-learned-behavior)
  - [Uninstallation](#uninstallation)
  - [Using your OpenAI API key](#using-your-openai-api-key)
  - [Using your Gemini API key](#using-your-gemini-api-key)
  - [Using a custom OpenAI-compatible API](#using-a-custom-openai-compatible-api)
  - [Testing](#testing)
    - [Optional headless live LLM tests](#optional-headless-live-llm-tests)
  - [Diagnostics](#diagnostics)
  - [Help and onboarding](#help-and-onboarding)
  - [How to Use](#how-to-use)
  - [Sorting a Remote Directory (e.g., NAS)](#sorting-a-remote-directory-eg-nas)
  - [Contributing](#contributing)
  - [Credits](#credits)
  - [License](#license)
  - [Donation](#donation)

---

## Technical reference

The main README stays focused on installation, features, and normal everyday
use. For contributor-facing and integration-facing details that are too deep for
the main entry page, use these technical references:

- [Architecture](docs/architecture.md)
- [Headless runtime contract](docs/headless-runtime-contract.md)
- [Configuration and environment](docs/configuration-and-environment.md)
- [Categorization behavior](docs/categorization-behavior.md)
- [Testing](docs/testing.md)
- [Updater contract](docs/updater-contract.md)

---

## Changelog

## [1.9.0] - 2026-07-03

- Added smart branching whitelists so categories can have their own allowed subcategories, with an improved whitelist editor that keeps global and category-specific subcategory modes mutually exclusive.
- Added structured project-folder protection for recursive scans, covering Unity, Unreal, Godot, Blender, Git repositories, and common source-code project layouts.
- Added file preview support in the Categorization Review dialog and improved accessibility labels/progress announcements for screen readers.
- Added custom visual model support, configurable local model storage, and improved reuse of already-downloaded Gemma 3 4B model files.
- Improved category and filename consistency by localizing suggested filenames, preserving UTF-8 metadata, stripping inline subcategory artifacts, and keeping date suffixes out of canonical cache labels.
- Improved remote LLM handling with rate-limit/backoff parsing and optional request pacing.
- Improved local runtime and release packaging reliability across Windows, Linux, and macOS, including safer backend probing, CUDA/Vulkan fallback handling, RPM packaging, and verified macOS release helpers.

## [1.8.0] - 2026-05-10

- Added backend status indicator to the status bar.
- The app now runs as a single instance - opening it again brings the existing window to the front instead of starting a second copy.
- Restored the app launcher for the non-Microsoft Store versions of the app and improved GPU selection, now preferring CUDA over Vulkan when both are available.
- Improved local GPU startup and local visual model handling for better reliability and compatibility.
- Added Gemma 3 4B IT and set it as the default visual model.
- Added Gemma 3 4B IT and Gemma 1.1 7B as built-in local categorization model choices, replacing LLaMa 3B.
- Improved image categorization quality and consistency by preserving image descriptions, using richer prompt context, adding special handling for screenshots and UI captures, and reducing drift between category labels.
- Improved image analysis stability, fallback behavior, and model-download validation.
- Added options to clear categorization and app caches, including a deeper reset of stored categorization state.
- Added local learning from your review decisions to improve future suggestions.
- Added localized Quick Start help, an FAQ link, and additional interface languages including Hindi, Swedish, Icelandic, Norwegian, Finnish, Danish, and Simplified Chinese.

See [CHANGELOG.md](CHANGELOG.md) for the full history.

---

## Features

- **AI-powered categorization**: Sort files using either local AI models on your computer or remote models with your own API key.
- **Offline-Friendly**: Use a local LLM to categorize files entirely - no internet or API key required.
- **Robust categorization**: Built-in rules and category matching help keep results more consistent across runs.
- **Configurable categorization controls**: Use whitelists, taxonomy normalization, consistency modes, and review-time edits to steer categories and subcategories.
- **Two categorization modes**: Pick **More Refined** for more specific labels with less pressure to stay in broad default categories, or **More Consistent** for steadier top-level categories across similar files.
- **Category whitelists**: Define named whitelists of allowed categories/subcategories, including smart branching lists where each main category has its own allowed subcategories. Manage them under **Settings → Manage category whitelists…**, then toggle/select them in the main window when you want to constrain model output for a session.
- **Category and rename languages**: Categories are chosen in English behind the scenes and then shown in your selected category language. Suggested filenames for images, documents, and supported audio/video files are localized the same way. The available languages depend on the selected local model.
- **Custom local LLMs**: Register your own local GGUF models directly from the **Select LLM** dialog. Add a matching MMProj file to make a custom model available for image analysis as well.
- **Image content analysis (Visual LLM)**: Analyze supported picture files with built-in visual backends such as the default Gemma 3 4B IT and LLaVA 1.6 Mistral 7B, with special handling for screenshots and UI captures so categories describe on-screen content more accurately (rename-only mode supported).
- **Image date-to-category suffix (optional)**: Append image creation date metadata to image category names when available.
- **Document content analysis (Text LLM)**: Analyze supported document files to summarize content and suggest filenames; uses the same selected LLM (local or remote).
- **Audio/video metadata filename suggestions**: Turn embedded media tags into clean, library-style filenames for supported audio and video files, with full review before anything is renamed.
- **Sortable review**: Sort the Categorization Review table by file name, category, or subcategory to triage faster.
- **Qt6 Interface**: Lightweight and responsive UI with refreshed menus and icons.
- **Interface languages**: English, Danish, Dutch, Finnish, French, German, Hindi, Icelandic, Italian, Korean, Norwegian, Simplified Chinese, Spanish, Swedish, and Turkish.
- **Cross-Platform Compatibility**: Works on Windows, macOS, and Linux.
- **Local Database Caching**: Speeds up repeated categorization, preserves approved labels and rename suggestions, and provides recent-category hints for consistency.
- **Local learning from approved reviews**: Approved category decisions can be stored locally and reused as hints for future runs without modifying the underlying model.
- **Cache maintenance tools**: Use **Settings → Clear cache…** to inspect and clear categorization cache, image location cache, and logs, or **Settings → Reset learned behavior…** to remove the separate learned-review database.
- **Sorting Preview**: See how files will be organized before confirming changes.
- **Dry run** / preview-only mode to inspect planned moves without touching files.
- **Persistent Undo** ("Undo last run") even after closing the sort dialog.
- **Project-folder protection**: Recursive scans skip recognized structured project roots such as Unity, Unreal, Godot, Blender project folders, Git repositories, and common source-code projects so files that depend on project-relative paths are not moved independently.
- **Bring your own remote credentials**: Store your OpenAI key, Gemini key, or custom OpenAI-compatible endpoint details locally for reuse in later runs.
- **Update Notifications**: Get notified about updates - with optional or required update flows.
- **Storage plugin support**: Install provider-specific compatibility modes from the **Plugins** menu when the app detects supported cloud-backed folders.
- **In-app help**: Open the localized **Help → Quick Start Guide** for a guided walkthrough or **Help → FAQ** for troubleshooting and common questions.

---

## Categorization

### Categorization modes

- **More refined**: The flexible, detail-oriented mode. It is less tied to the default category structure, so it can choose a more specific top-level category when that fits better. For example: `Security / PCI DSS`, `Manuals / Camera Guides`, or `Wildlife / Lions`. This works well for mixed, specialized, or long-tail folders.
- **More consistent**: The uniform mode. It uses recent similar results to favor stable broad folder categories, so related files are more likely to land under the same top-level folders. For example: `Documents / PCI DSS`, `Documents / Camera Guides`, or `Images / Lions`. This works well when you want a cleaner, more uniform folder layout.
- Switch between the two via the **Categorization type** radio buttons on the main window; your choice is saved for the next run.

Example without a whitelist:

```text
More refined
- pci_dss_quick_reference.pdf -> Security / PCI DSS
- camera_setup_manual.pdf -> Manuals / Camera Guides
- lion_photo.jpg -> Wildlife / Lions

More consistent
- pci_dss_quick_reference.pdf -> Documents / PCI DSS
- camera_setup_manual.pdf -> Documents / Camera Guides
- lion_photo.jpg -> Images / Lions
```

### Category language selection

- Category labels are chosen in English first behind the scenes, then shown in the category language you selected under **Settings → Category language**.
- Suggested filenames for image, document, and supported audio/video rename flows are also localized into the selected **Settings → Category language** target before review/apply.
- The list is model-dependent for built-in local models. **Gemma 3 4B IT** and **Custom** local models expose the full app-supported category-language list, **Mistral 7B** exposes a smaller supported subset, and **Gemma 1.1 7B** stays English-only.
- When the supported list is long, the menu is grouped into alphabetical submenus to keep it usable on smaller screens.

### Category whitelists

- Enable **Use a whitelist** to apply the selected category list during categorization; disable it to let the model choose more freely.
- Manage lists (add, edit, remove) under **Settings → Manage category whitelists…**. Built-in `Default` and `Documents` lists are auto-created only when no lists exist, and multiple named lists can be kept for different projects.
- The whitelist editor has three sections. **Main categories / top-level folders** defines the destination category folders. **Global subcategories** defines subcategories that may be used under any main category. **Category-specific subcategories** defines smart branching rows where each main category has its own allowed subcategories.
- **Global subcategories** and **Category-specific subcategories** are alternatives. If you enter values in one section, the other section is disabled/ignored for that whitelist so the app does not mix two incompatible constraint styles.
- Smart branching is useful when the same broad top-level folders should contain different allowed subfolders. For example, `Documents -> Invoices, Receipts, Taxes` and `Images -> Screenshots, Photos` tells the app that `Screenshots` is valid under `Images`, but not under `Documents`.
- The built-in `Documents` whitelist uses smart branching: the only top-level category is `Documents`, while topics such as invoices, receipts, taxes, contracts, reports, and notes are stored as `Documents` subcategories.
- Narrow whitelists are still easiest for smaller local models, but large whitelists no longer need to dump every label into every prompt. When needed, the app reduces large and smart branching whitelists to relevant candidates plus valid category/subcategory pair guidance.
- Whitelists apply in either categorization mode; pair them with **More consistent** when you want the strongest adherence to a constrained vocabulary.

---

## Image analysis (Visual LLM)

Image analysis can run locally to understand what a picture shows and suggest a better category or filename. It does not require an API key.

As of 1.9.0, **Gemma 3 4B IT** is the default visual backend. The app also gives screenshots, webpage captures, dashboards, forms, mockups, and other UI-like images extra prompt guidance so categories describe what is shown on screen instead of misclassifying the image as the software artifact itself.

The app currently exposes two built-in visual backends: the default Gemma 3 4B IT and LLaVA 1.6 Mistral 7B. In the current embedded runtime, all supported local visual backends require two GGUF files: the main text model and a matching `mmproj` projector file.

The Gemma 3 4B IT GGUF is also available as a built-in local text/categorization model. When used only for categorization or document analysis, it runs as a normal text model and does not need `mmproj`. If you already downloaded the Gemma 3 text GGUF for image analysis, the local text-model entry reuses that same file automatically. The extra `mmproj` file is only required for visual image analysis.

Custom local GGUF models can also be used for image analysis when their custom LLM entry includes a matching MMProj file. Those entries appear in the **Visual model** selector as custom visual backends and use the files you selected instead of the built-in download controls.

### Required visual LLM files

The **Select LLM** dialog includes an "Image analysis models" section with backend-specific downloads:

- **Visual text model (GGUF)**: The language model that produces the description and the filename suggestion.
- **Matching `mmproj` file (GGUF)**: The multimodal projector that maps image embeddings into the model token space so the backend can accept images.

Both files are required for the selected backend. If either one is missing, image analysis is disabled and the app will prompt to open the **Select LLM** dialog to download them. The download URLs can be overridden with backend-specific environment variables such as `LLAVA_MODEL_URL` / `LLAVA_MMPROJ_URL` or `GEMMA3_4B_MODEL_URL` / `GEMMA3_4B_MMPROJ_URL` (see [Environment variables](#environment-variables)).

The same dialog also includes **Model storage**, which changes where built-in local text and visual model downloads are stored. Leave it empty to use the platform default.

### Main window options

Image analysis adds six related checkboxes to the main window:

- **Analyze picture files by content**: Runs the visual LLM on supported picture files and reports progress in the analysis dialog.
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
- **Optional GPU backends**: CUDA 12.x for NVIDIA cards or a Vulkan 1.2+ runtime. On Windows installer/standalone builds, `aifilesorter.exe` auto-detects the best available backend and now prefers CUDA over Vulkan when both are available, falling back to CPU/OpenBLAS automatically. On Linux, the same applies through `run_aifilesorter.sh`; when a dedicated CPU runtime bundle is absent, the launcher can also reuse the staged Vulkan payload for CPU/OpenBLAS fallback, so CUDA is never required to run the app.
- **Git** (optional): For cloning this repository. Archives can also be downloaded.
- **Remote model credentials** (optional): Required only when using ChatGPT, Gemini, or a custom OpenAI-compatible API endpoint.

---

## Installation

File categorization with local LLMs is completely free of charge. If you prefer to use a remote workflow (ChatGPT, Gemini, or a custom OpenAI-compatible endpoint) you will need your own API credentials or endpoint configuration with a suitable quota or local server setup (see [Using your OpenAI API key](#using-your-openai-api-key), [Using your Gemini API key](#using-your-gemini-api-key), or [Using a custom OpenAI-compatible API](#using-a-custom-openai-compatible-api)).

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
   GPU acceleration additionally requires either a working Vulkan 1.2+ stack (Mesa, AMD/Intel/NVIDIA drivers) or, for NVIDIA users, the matching CUDA runtime (`nvidia-cuda-toolkit` or vendor packages). The launcher automatically prefers CUDA when both are present and falls back to CPU if neither is available.
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
      zlib1g-dev patchelf
    ```
   - Fedora / RHEL:

    ```bash
    export PATH="/usr/lib64/qt6/libexec:$PATH"
    sudo dnf install -y gcc-c++ cmake git pkgconf-pkg-config qt6-qtbase-devel qt6-qttools-devel \
      libcurl-devel jsoncpp-devel sqlite-devel openssl-devel fmt-devel spdlog-devel libmediainfo-devel openblas-devel patchelf
    ```

    `openblas-devel` provides the OpenBLAS headers/libs used for the CPU baseline and for
    explicit `blas=on` llama builds on Fedora.

    If you plan to build the Vulkan llama runtime on Fedora, also install the Vulkan tools
    and shader compiler, plus a working Vulkan driver/runtime:

    ```bash
    sudo dnf install -y vulkan-tools glslc
    # AMD / Intel / Mesa-based Vulkan runtime
    sudo dnf install -y mesa-vulkan-drivers
    # NVIDIA proprietary driver users should install the vendor Vulkan runtime instead
    vulkaninfo >/dev/null
    ```

    If you plan to build the CUDA llama runtime on Linux, install an NVIDIA driver plus the
    full NVIDIA CUDA Toolkit (not just Python wheels/runtime packages), then verify both the
    driver and compiler in the same shell you will use for the build:

    ```bash
    export PATH="/usr/local/cuda/bin:$PATH"
    nvidia-smi
    nvcc --version
    ```

    Debian/Ubuntu users can use the distro-packaged toolkit (`nvidia-cuda-toolkit`) if it
    matches the driver/runtime they intend to build against. The Linux helper can also fall
    back to the toolkit's `libcuda` stubs when `libcuda.so.1` is not on the default linker
    path, which is useful on CI or build-only hosts, but running the CUDA backend still
    requires a real NVIDIA driver installation. Fedora/RHEL users should use a supported CUDA
    Toolkit install from NVIDIA's Linux installation guide because the NVIDIA CUDA repository
    setup varies by release. On Fedora, enable the NVIDIA CUDA repository first or
    `sudo dnf install cuda-toolkit` will fail with "No match for argument":

    ```bash
    distro="fedora$(rpm -E %fedora)"
    sudo dnf install -y dnf-plugins-core
    sudo dnf config-manager addrepo \
      --from-repofile="https://developer.download.nvidia.com/compute/cuda/repos/${distro}/x86_64/cuda-${distro}.repo"
    sudo dnf clean expire-cache
    ```

    Once that repository is enabled for your Fedora release, the toolkit install itself is:

    ```bash
    sudo dnf install -y cuda-toolkit
    ```

    If you also want NVIDIA's packaged driver path from the same guide rather than a
    separately installed proprietary driver, the documented Fedora command is:

    ```bash
    sudo dnf module install -y nvidia-driver:latest-dkms
    ```

    The official Fedora instructions, including repo enablement and RPMFusion caveats, are:
    <https://docs.nvidia.com/cuda/cuda-installation-guide-linux/index.html>

    On Linux, the helper also needs a CUDA-supported `g++` host compiler. It auto-selects
    `/usr/bin/g++-15`, `g++-14`, `g++-13`, `g++-12`, `g++-11`, `g++-10`, then `g++`, and
    will stop with an error if none of those are suitable. Current CUDA 13.2 supports GCC
    6.x through 15.x. If your installed CUDA release supports an older range than your distro
    default compiler, install a compatibility package such as Fedora `gcc14-c++` or
    `gcc13-c++` and rerun with `NVCC_CCBIN=/usr/bin/g++-14` or `NVCC_CCBIN=/usr/bin/g++-13`
    in the same shell.

   - Arch / Manjaro:

    ```bash
     sudo pacman -S --needed base-devel git cmake qt6-base qt6-tools curl jsoncpp sqlite openssl fmt spdlog mediainfo patchelf
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

   The Linux helper accepts either `cuda=on` / `vulkan=on` / `blas=on` or the GNU-style
   `--cuda=on` / `--vulkan=on` / `--blas=on` forms.

   ```bash
   # CPU / OpenBLAS
   ./app/scripts/build_llama_linux.sh cuda=off vulkan=off
   # CUDA (optional; runtime use requires NVIDIA driver + full CUDA Toolkit; verify
   # `nvidia-smi` and `nvcc --version` first. If `nvcc` is not on PATH after install,
   # run `export PATH=/usr/local/cuda/bin:$PATH` in this shell. The Linux helper can
   # use toolkit libcuda stubs on build-only hosts, and it also needs a CUDA-supported
   # g++ host compiler, preferring /usr/bin/g++-15 first.)
   ./app/scripts/build_llama_linux.sh cuda=on vulkan=off
   # Vulkan (optional; requires a working Vulkan 1.2+ stack and glslc; on Fedora install
   # vulkan-tools + glslc and ensure `vulkaninfo` succeeds. Mesa-based systems also need
   # mesa-vulkan-drivers.)
   ./app/scripts/build_llama_linux.sh cuda=off vulkan=on
   ```

   Each invocation stages the corresponding `llama`/`ggml` libraries under `app/lib/precompiled/<variant>` and the runtime DLL/SO copies under `app/lib/ggml/w<variant>`. The script refuses to enable CUDA and Vulkan simultaneously, so run it separately for each backend. Shipping both directories lets the launcher pick CUDA when available, then Vulkan, and otherwise stay on CPU. If you only ship the Vulkan payload, the Linux launcher can reuse its bundled CPU/OpenBLAS libraries as the CPU fallback, matching the Windows launcher behavior.

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

8. **Build an RPM package (optional; Fedora / RHEL)**

   ```bash
   ./app/scripts/create_rpm.sh
   ```

   The RPM packaging script follows the same runtime-selection model as the Debian packager:
   CPU is always bundled, staged CUDA/Vulkan runtime dirs are auto-included when present, and
   the installed `run_aifilesorter.sh` launcher chooses CUDA first, then Vulkan, then CPU at
   runtime. Use `./app/scripts/create_rpm.sh --cpu-only` for a smaller CPU-only package, or
   `--include-vulkan` / `--include-cuda` if you want the script to fail when a specific
   staged variant is missing. The RPM payload is built under `dist/aifilesorter_rpm/`.

### macOS

Apple Silicon Macs running macOS 15 or later are supported for macOS source builds and release bundles.

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
   > The macOS build pins `MACOSX_DEPLOYMENT_TARGET=15.0` so the Mach-O `LC_BUILD_VERSION` matches the supported Apple Silicon floor. Raise it if you intentionally want a newer floor.

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
   ```

   These targets rebuild the llama.cpp runtime before compiling the app.
   `sudo make install` places the macOS runtime libraries under `/usr/local/lib/aifilesorter` to avoid collisions with unrelated system or Homebrew ggml libraries.
   The commands above build the raw executable only; they do **not** currently create a distributable `.app` bundle or `.dmg`.
   This repository does not yet ship a documented or automated macOS bundle/DMG packaging target in `README.md`, so any `.app` / `.dmg` release packaging must be handled as a separate macOS-hosted release step.
   Each variant uses distinct build directories to avoid cross-arch collisions:
   - llama.cpp libs: `app/lib/precompiled-m1`, `app/lib/precompiled-m2`
   - object files: `app/obj/arm64`

### Windows

Build now targets native MSVC + Qt6 without MSYS2. Two options are supported; the vcpkg route is simplest.

Option A - CMake + vcpkg (recommended)

1. Install prerequisites:
   - Visual Studio 2022 or newer with Desktop C++ workload
   - CMake 3.21+ (Visual Studio ships a recent version)
   - vcpkg: <https://github.com/microsoft/vcpkg> (clone and bootstrap)
   - package-managed `libmediainfo` via vcpkg manifest (no vendored MediaInfo submodule/binaries)
   - **MSYS2 UCRT64 or MINGW64 + OpenBLAS**: install MSYS2 from <https://www.msys2.org>, open either an *MSYS2 UCRT64* or *MSYS2 MINGW64* shell, and run `pacman -S --needed mingw-w64-ucrt-x86_64-openblas` or `pacman -S --needed mingw-w64-x86_64-openblas`. The `build_llama_windows.ps1` script probes `C:\msys64\ucrt64` and `C:\msys64\mingw64` automatically, and also supports overriding the location with `openblasroot=<path>` or `OPENBLAS_ROOT`.
   - **If you plan to build the CUDA runtime on Windows**: install the current NVIDIA driver and the full NVIDIA CUDA Toolkit from <https://developer.nvidia.com/cuda-downloads>. After installation, open a fresh **x64 Native Tools / Visual Studio Developer PowerShell** and verify the toolkit/driver before running the helper:

     ```powershell
     $env:CUDA_PATH
     nvcc --version
     nvidia-smi
     ```

     The Windows helper expects a full toolkit install with `nvcc`, headers, and libraries.
     Conda or pip CUDA runtime packages are not enough for building the bundled `llama.cpp`
     CUDA variant. NVIDIA's official Windows installation guide is here:
     <https://docs.nvidia.com/cuda/cuda-installation-guide-microsoft-windows/index.html>
2. Clone repo and submodules:

   ```powershell
   git clone https://github.com/hyperfield/ai-file-sorter.git
   cd ai-file-sorter
   git submodule update --init --recursive
   ```

3. **Build vendored libzip** (generates `zipconf.h` and `libzip.lib`)

   Run from the same x64 Native Tools / Visual Studio Developer PowerShell you will use to build the app:

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

4. Determine your vcpkg root only if auto-discovery does not find it. The Windows helper scripts look in this order: `VCPKG_ROOT` / `VPKG_ROOT`, `vcpkg` / `vpkg` on `PATH`, then common writable locations such as `<repo-drive>:\dev\vcpkg`, `<repo-drive>:\vcpkg`, `%SystemDrive%\dev\vcpkg`, and `%SystemDrive%\vcpkg`.
    - If `vcpkg` is on your `PATH`, run this command to print the location:

      ```powershell
      Split-Path -Parent (Get-Command vcpkg).Source
      ```

    - Otherwise use the directory where you cloned vcpkg, or pass it explicitly to the helper scripts.

   MediaInfo note: you do **not** manually add `MediaInfoLib` include/lib paths on Windows. The project already declares `libmediainfo` in `app/vcpkg.json`, and `app\build_windows.ps1` configures CMake with the vcpkg toolchain + manifest so `find_package(MediaInfoLib ...)` resolves it automatically. If you want to preinstall or verify it explicitly, run `vcpkg install libmediainfo:x64-windows`.
5. Build the bundled `llama.cpp` runtime variants (run from the same **x64 Native Tools** / **Visual Studio Developer PowerShell** shell). Invoke the script once per backend you need. The script accepts `cuda=on|off`, `vulkan=on|off`, `blas=on|off`, `vcpkgroot=<path>`, `openblasroot=<path>`, and `cudaarch=<value>`. `vcpkgroot=<path>` is optional and only needed when auto-discovery misses your install. `blas` defaults to `AUTO`: it is enabled automatically for CPU-only builds and disabled automatically for CUDA/Vulkan builds unless you force it on. For CUDA builds, the helper prefers a valid `CUDA_PATH` and otherwise auto-selects the newest installed toolkit it can validate under `C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA`. Make sure `nvcc --version` and `nvidia-smi` succeed in this same shell before running `cuda=on`. Make sure the MSYS2 OpenBLAS install from step 1 is present before running the CPU-only variant (or pass `openblasroot=<path>` explicitly):

   ```powershell
   # CPU / OpenBLAS only
   app\scripts\build_llama_windows.ps1 cuda=off vulkan=off
   # CUDA (requires matching NVIDIA toolkit/driver)
   app\scripts\build_llama_windows.ps1 cuda=on vulkan=off
   # CUDA + OpenBLAS (optional override if you want that combination explicitly)
   app\scripts\build_llama_windows.ps1 cuda=on vulkan=off blas=on
   # Vulkan (requires LunarG Vulkan SDK or vendor Vulkan 1.2+ runtime)
   app\scripts\build_llama_windows.ps1 cuda=off vulkan=on
   ```
  
  Each run emits the appropriate `llama.dll` / `ggml*.dll` pair under `app\lib\precompiled\<cpu|cuda|vulkan|vulkan-blas>` and copies the runtime DLLs into the Windows runtime directories used by the app (`app\lib\ggml\wocuda`, `app\lib\ggml\wcuda`, or `app\lib\ggml\wvulkan`). The helper also stages OpenSSL runtime DLLs and, for OpenBLAS-backed builds, the MinGW companion DLLs needed beside `libopenblas.dll`. If the vendored `llama.cpp` UI assets are missing, the helper downloads a prebuilt UI bundle before configuring and removes the temporary extracted assets after the build completes. For Vulkan builds, install the latest LunarG Vulkan SDK (or the vendor's runtime), ensure `vulkaninfo` succeeds in the same shell, and then run the script. The Windows launcher `aifilesorter.exe` auto-selects the best backend at launch: CUDA is preferred, Vulkan is used when CUDA is unavailable, and CPU remains the fallback.

6. Build the Qt6 application using the helper script (still in the VS shell). The helper stages runtime DLLs via `windeployqt` and shares one dependency install tree across builds:

   ```powershell
   # One-time per shell if script execution is blocked:
   Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass

   app\build_windows.ps1 -Configuration Release
   ```

   - Pass `-VcpkgRoot <path>` only if auto-discovery misses your vcpkg install. The path must contain `scripts\buildsystems\vcpkg.cmake`.
   - Pass `-Generator <name>` only if you need to override the auto-selected CMake generator. By default the helper picks the newest installed Visual Studio generator it can validate and can fall back to the Visual Studio-bundled `cmake.exe` when needed.
   - The main bundled output is staged under `app\build-windows\Release`.
   - `aifilesorter.exe` is the Windows entry point for the bundled build and launches `aifilesorter-bin.exe`.
   - `-VcpkgRoot` is optional if `VCPKG_ROOT`/`VPKG_ROOT` is set or `vcpkg`/`vpkg` is on `PATH`.
   - Pass `-SkipDeploy` if you only want the binaries without bundling runtime DLLs.
   - Pass `-Parallel <N>` to override the default "all cores" parallel build behaviour (for example, `-Parallel 8`). By default the script invokes `cmake --build ... --parallel <core-count>` and `ctest -j <core-count>` to keep both MSBuild and Ninja fully utilized.

Option B - CMake + Qt online installer

1. Install prerequisites:
   - Visual Studio 2022 or newer with Desktop C++ workload
   - Qt 6.x MSVC kit via Qt Online Installer (e.g., Qt 6.6+ with MSVC 2019/2022)
   - CMake 3.21+
   - vcpkg (for non-Qt libs): curl, jsoncpp, sqlite3, openssl, fmt, spdlog, gettext, libmediainfo
2. **Build vendored libzip** (generates `zipconf.h` and `libzip.lib`)

   Run from the same x64 Native Tools / Visual Studio Developer PowerShell you will use to build the app:

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

3. Build the bundled `llama.cpp` runtime (same VS shell). The helper resolves cURL and OpenSSL from vcpkg, and OpenBLAS from an MSYS2 UCRT64 or MINGW64 install:

   ```powershell
   pwsh .\app\scripts\build_llama_windows.ps1 [cuda=on|off] [vulkan=on|off] [blas=on|off] [vcpkgroot=<path>] [openblasroot=C:\msys64\ucrt64] [cudaarch=<value>]
   ```

   `blas` defaults to `AUTO`, which means ON for CPU-only builds and OFF for CUDA/Vulkan builds unless you force it. This is required before configuring the GUI because the build links against the produced `llama` import libraries and runtime DLLs.
4. Configure CMake from the repo root so CMake sees both the Qt install and the app's vcpkg manifest (adapt `CMAKE_PREFIX_PATH` to your Qt install):

    ```powershell
    $env:VCPKG_ROOT = "D:\path\to\vcpkg"
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

- To rebuild from scratch, run `.\app\build_windows.ps1 -Clean`. The script removes the Windows build directories used by the helper and the shared `app\build-windows-vcpkg_installed` dependency tree before configuring.
- Runtime DLLs are copied automatically via `windeployqt` after each successful build; skip this step with `-SkipDeploy` if you manage deployment yourself.
- If Visual Studio sets `VCPKG_ROOT` to its bundled copy under `Program Files`, point `VCPKG_ROOT` to a writable clone or pass `vcpkgroot=<path>` when running `build_llama_windows.ps1`. The script skips the bundled Visual Studio copy during auto-discovery because it is usually read-only.
- If a Windows build directory was previously configured with a different generator, platform, or Visual Studio instance, `app\build_windows.ps1` now detects the stale cache and recreates that build directory automatically before configuring.
- If you plan to ship CUDA or Vulkan acceleration, run the `build_llama_*` helper for each backend you intend to include before configuring CMake so the libraries exist. The runtime can carry both and auto-select at launch, so CUDA remains optional.
- `-BuildTests` and `-RunTests` are intended for the primary bundled Windows build configuration.

### Running tests

Catch2-based unit tests are optional. Enable them via CMake:

```bash
cmake -S app -B build-tests -DAI_FILE_SORTER_BUILD_TESTS=ON -DAI_FILE_SORTER_REQUIRE_MEDIAINFOLIB=ON
cmake --build build-tests --parallel $(nproc)
ctest --test-dir build-tests --output-on-failure -j $(nproc)
```

On macOS, replace `$(nproc)` with `$(sysctl -n hw.ncpu)`.

On Windows (PowerShell), use:

```powershell
$env:VCPKG_ROOT = "D:\path\to\vcpkg"
$qt = "C:\Qt\6.6.3\msvc2019_64"  # example
$toolchain = Join-Path $env:VCPKG_ROOT "scripts\buildsystems\vcpkg.cmake"
cmake -S app -B build-tests -G "Ninja" `
  -DCMAKE_PREFIX_PATH=$qt `
  "-DCMAKE_TOOLCHAIN_FILE=$toolchain" `
  -DVCPKG_MANIFEST_DIR=app `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DAI_FILE_SORTER_BUILD_TESTS=ON `
  -DAI_FILE_SORTER_REQUIRE_MEDIAINFOLIB=ON
cmake --build build-tests --config Release `
  --target ai_file_sorter_tests ai_file_sorter_updater_notify_only_tests ai_file_sorter_updater_disabled_tests `
  --parallel $env:NUMBER_OF_PROCESSORS
ctest --test-dir build-tests -C Release --output-on-failure -j $env:NUMBER_OF_PROCESSORS
```

Notes

- The easiest Windows path is still `app\build_windows.ps1 -Configuration Release -BuildTests -RunTests`, which wires up the same vcpkg manifest/toolchain expectations for the main bundled build.
- To register the optional live LLM CTest suite from the Windows helper, use `.\app\build_windows.ps1 -Configuration Release -Variants Standard -BuildTests -EnableLiveLlmTests` in PowerShell or `app\build_windows.cmd -Configuration Release -Variants Standard -BuildTests -EnableLiveLlmTests` in `cmd.exe`. Add `-RunTests` only if you want the full registered CTest set to run immediately.
- If you use a Visual Studio multi-config build tree such as `build-tests-vs`, building only `--target aifilesorter` produces `Release\aifilesorter-bin.exe`. The launcher `Release\aifilesorter.exe` belongs to the `StartAiFileSorter` target, so build `StartAiFileSorter` explicitly or omit `--target` if you want the launcher too.
- If CMake reports a missing hardcoded Visual Studio instance for `build-tests`, the path is coming from the existing build tree's `CMakeCache.txt`, not from this repo. Use `cmake --fresh`, delete/recreate that build directory, or choose a new build directory before changing generators or Visual Studio installations.
- List individual Catch2 cases: `./build-tests/ai_file_sorter_tests --list-tests`
- Print each case name (including successes): `./build-tests/ai_file_sorter_tests --verbosity high --success`

On Windows you can pass `-BuildTests` (and `-RunTests` to execute `ctest`) to `app\build_windows.ps1`:

```powershell
.\app\build_windows.ps1 -Configuration Release -Variants Standard -BuildTests -RunTests
```

From `cmd.exe`, use `app\build_windows.cmd -Configuration Release -Variants Standard -BuildTests -RunTests` instead.

The current suite (under `tests/unit`) focuses on core utilities; expand it as new functionality gains coverage.

#### Optional headless live LLM tests

The normal Catch2/CTest suite does not launch a real local model against real files. For that purpose, use the opt-in live runner at `tests/live_llm/headless_live_llm_tests.py`. It invokes the production headless command, seeds an isolated app config with your local GGUF model, creates per-run fixture folders, downloads/caches a few small public PDF/image fixtures, and validates status JSON plus filesystem effects.

These tests are intentionally not enabled by default because they are slow, model-dependent, and partly nondeterministic. Missing live-test prerequisites return exit code `77`, which CTest treats as skipped.

Run sequence with CTest:

```powershell
# 1. Windows recommended path from PowerShell: configure/build with stale-cache detection.
.\app\build_windows.ps1 -Configuration Release -Variants Standard -BuildTests -EnableLiveLlmTests

# 2. Point the runner at a local text GGUF model.
#    You can skip this if the normal AI File Sorter settings already select
#    an available local/custom GGUF model.
$env:AI_FILE_SORTER_LIVE_LLM_MODEL = "C:\models\text-model.gguf"

# 3. Optional: enable image-content rename cases with a visual model pair.
$env:AI_FILE_SORTER_LIVE_VISUAL_MODEL = "C:\models\vision-model.gguf"
$env:AI_FILE_SORTER_LIVE_VISUAL_MMPROJ = "C:\models\mmproj-model.gguf"

# 4. Optional: force a backend. Values: auto, cpu, cuda, vulkan.
$env:AI_FILE_SORTER_LIVE_BACKEND = "cuda"

# 5. Run only the live LLM suite from the helper's Standard build directory.
ctest --test-dir app\build-windows -C Release -L live-llm --output-on-failure

# Use -V instead when you want live per-case progress in the terminal.
ctest --test-dir app\build-windows -C Release -L live-llm -V
```

From `cmd.exe`, use `app\build_windows.cmd -Configuration Release -Variants Standard -BuildTests -EnableLiveLlmTests` for step 1.

On bundled Windows builds, the live suite runs through `aifilesorter.exe`, not `aifilesorter-bin.exe`, so backend DLL selection matches the normal non-Store launcher. If an older CTest file still passes `aifilesorter-bin.exe`, the Python runner automatically switches to the sibling launcher when it exists. Use `AI_FILE_SORTER_LIVE_BACKEND=cuda` to validate CUDA specifically, leave it unset for the launcher's normal CUDA -> Vulkan -> CPU selection, or use `cpu` for deterministic CPU/OpenBLAS runs.

CTest suppresses passing-test output unless `-V` is used. The runner also writes a live progress log and a pointer file, so a second PowerShell can tail progress while a non-verbose CTest run is active:

```powershell
$work = Get-Content "$env:TEMP\aifs-live-llm-latest.txt"
Get-Content (Join-Path $work "progress.log") -Wait
```

Verbose progress includes the backend environment received by the headless text LLM client, for example `Local text LLM backend request: AI_FILE_SORTER_GPU_BACKEND=cuda`. If that line says `cpu`, clear any stale `AI_FILE_SORTER_LIVE_BACKEND=cpu` setting and rebuild after stopping old test processes that may still hold `aifilesorter-bin.exe`.

The live runner colorizes terminal output by default: failures are red, warnings/skips are yellow, progress/info is blue, and passes are green. Set `AI_FILE_SORTER_LIVE_COLOR=never` or pass `--color never` for plain output.
Long-running cases refresh an inline `elapsed=NNNs` counter on the active `RUN` line instead of printing heartbeat lines; set `AI_FILE_SORTER_LIVE_INLINE_PROGRESS=never` if a log collector needs plain one-line records.

Manual Windows CMake users should configure from an x64 Visual Studio Developer PowerShell with an explicit generator and toolchain, for example `cmake -S app -B build-tests -G "Ninja" -DCMAKE_PREFIX_PATH=$qt "-DCMAKE_TOOLCHAIN_FILE=$toolchain" -DVCPKG_MANIFEST_DIR=app -DVCPKG_TARGET_TRIPLET=x64-windows -DAI_FILE_SORTER_BUILD_TESTS=ON -DAI_FILE_SORTER_ENABLE_LIVE_LLM_TESTS=ON`. Use `cmake --fresh` when supported, delete/recreate the build directory, or choose a new build directory if `build-tests` was previously configured with another generator or Visual Studio instance.

On Linux/macOS, configure with `cmake -S app -B build-tests -DAI_FILE_SORTER_BUILD_TESTS=ON -DAI_FILE_SORTER_ENABLE_LIVE_LLM_TESTS=ON -DAI_FILE_SORTER_REQUIRE_MEDIAINFOLIB=ON`, build with your normal parallelism value, set `export AI_FILE_SORTER_LIVE_LLM_MODEL=/path/to/text-model.gguf` if needed, and omit `-C Release` for single-config build directories.

Run sequence without CTest:

```powershell
python tests\live_llm\headless_live_llm_tests.py `
  --app app\build-windows\Release\aifilesorter.exe `
  --model C:\models\text-model.gguf `
  --backend cpu `
  --keep-work-dir `
  --verbose
```

Useful direct-run filters:

```powershell
python tests\live_llm\headless_live_llm_tests.py --app app\build-windows\Release\aifilesorter.exe --model C:\models\text-model.gguf --backend cpu --only rename_documents
python tests\live_llm\headless_live_llm_tests.py --app app\build-windows\Release\aifilesorter.exe --model C:\models\text-model.gguf --backend cpu --only whitelist
```

Fixture behavior:

- Generated fixtures are created per run under a temporary work directory, or under `AI_FILE_SORTER_LIVE_WORK_DIR` / `--work-dir` if provided.
- Downloaded public fixtures are cached under `~/.cache/ai-file-sorter/live-fixtures` by default, or `AI_FILE_SORTER_LIVE_FIXTURE_CACHE` / `--fixture-cache`.
- No static binary fixture folder is committed to the repository; each test copies fixtures into isolated case directories with varied and international filenames.
- If `--model` / `AI_FILE_SORTER_LIVE_LLM_MODEL` is omitted, the runner reads AI File Sorter `config.ini` and uses the selected local/custom GGUF when available. Override that settings path with `AI_FILE_SORTER_LIVE_SETTINGS_FILE` / `--settings-file`.
- Runtime status updates include the selected `AI_FILE_SORTER_GPU_BACKEND`, `AI_FILE_SORTER_GGML_DIR`, and CUDA-disable state as reported by the launched headless process.
- Non-English rename cases report weak language/script localization as warnings by default. Set `AI_FILE_SORTER_LIVE_REQUIRE_LOCALIZED_RENAMES=1` for strict localized rename failures.
- Failed runs keep their generated work directory automatically so `runs\<case>\stdout.txt`, `stderr.txt`, `status.json`, and fixtures remain available for diagnosis.

Current live coverage includes categorization with and without subcategories, selected-file auto-apply boundaries, whitelist-restricted categorization, document renaming in English/French/Simplified Chinese/Hindi, optional image-content renaming in those languages, a FLAC metadata rename capability probe, and categorize-and-rename review-plan generation. See `tests/live_llm/README.md` for all runner options and fixture source URLs.

### Selecting a backend at runtime

Both the Linux launcher (`app/bin/run_aifilesorter.sh` / `aifilesorter-bin`) and the Windows launcher (`aifilesorter.exe` in bundled Windows builds) accept the following optional flags:

- `--cuda={on|off}` – force-enable or disable the CUDA backend.
- `--vulkan={on|off}` – force-enable or disable the Vulkan backend.

When no flags are provided the app auto-detects available runtimes in priority order (CUDA → Vulkan → CPU). Use the flags to skip a backend (`--cuda=off` forces Vulkan/CPU even if CUDA is installed, `--vulkan=off` tests CUDA explicitly) or to validate a newly installed stack (`--vulkan=on`). Passing `on` to both flags is rejected, and if neither GPU backend is detected the app automatically stays on CPU. On Linux, CPU mode can reuse the staged Vulkan runtime payload when the dedicated CPU payload is not present.

#### Vulkan and VRAM notes

- CUDA is preferred when available; Vulkan is used when CUDA is unavailable or explicitly requested.
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

- `LLAVA_MODEL_URL` - download URL for the LLaVA 1.6 Mistral 7B text model.
- `LLAVA_MMPROJ_URL` - download URL for the LLaVA 1.6 Mistral 7B mmproj file.
- `GEMMA3_4B_MODEL_URL` - download URL for the default/recommended Gemma 3 4B IT text model.
- `GEMMA3_4B_MMPROJ_URL` - download URL for the default/recommended Gemma 3 4B IT mmproj file.
- `AI_FILE_SORTER_VISUAL_USE_GPU` - force visual encoder GPU usage (`1`) or CPU (`0`). Defaults to auto; Vulkan may fall back to CPU if VRAM is low.

Timeouts and logging:

- `AI_FILE_SORTER_LOCAL_LLM_TIMEOUT` - seconds to wait for local LLM responses (default 60).
- `AI_FILE_SORTER_REMOTE_LLM_TIMEOUT` - seconds to wait for OpenAI/Gemini responses (default 10).
- `AI_FILE_SORTER_CUSTOM_LLM_TIMEOUT` - seconds to wait for custom OpenAI-compatible API responses (default 60).
- `AI_FILE_SORTER_REMOTE_REQUESTS_PER_MINUTE` - optional pacing limit for remote LLM calls. Use this for rate-limited providers such as OpenRouter; for example, `20` keeps requests within a 20 requests/minute quota. `0` or unset disables pacing. The same value can also be set in `config.ini` as `RemoteRequestsPerMinute` under `[Settings]`.
- `AI_FILE_SORTER_LLAMA_LOGS` - enable verbose llama.cpp logs (`1`/`true`); also honors `LLAMA_CPP_DEBUG_LOGS`.

Storage and updates:

- `AI_FILE_SORTER_CONFIG_DIR` - override the base config directory (where `config.ini` lives).
- `AI_FILE_SORTER_LLM_STORAGE_DIR` - override where downloaded local LLM files are stored. This can also be set from **Settings → Select LLM… → Model storage**.
- `AI_FILE_SORTER_LLM_DIR` - legacy alias for `AI_FILE_SORTER_LLM_STORAGE_DIR`.
- `CATEGORIZATION_CACHE_FILE` - override the SQLite cache filename inside the config dir.
- `UPDATE_SPEC_FILE_URL` - primary update feed spec URL used for normal runs. The updater now reads per-platform streams from `update.windows`, `update.macos`, and `update.linux`, with legacy single-stream feeds still accepted. Each stream may also include its own `changelog` list for the update dialog.
- `UPDATE_SPEC_FILE_URL_DEVELOPMENT` - alternate update feed spec URL used when the app starts with `--development`. If this value is unset, development mode falls back to `UPDATE_SPEC_FILE_URL`.
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
    "changelog": [
      "General compatibility fixes for older clients"
    ],
    "windows": {
      "current_version": "1.7.1",
      "min_version": "1.6.0",
      "download_url": "https://filesorter.app/download",
      "changelog": [
        "Improved installer handoff on Windows",
        "Added more update details in the dialog"
      ],
      "installer_url": "https://filesorter.app/downloads/AIFileSorterSetup-1.7.1.exe",
      "installer_sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    },
    "macos": {
      "current_version": "1.7.1",
      "min_version": "1.6.0",
      "download_url": "https://filesorter.app/download",
      "changelog": [
        "Updated notarized package metadata"
      ]
    },
    "linux": {
      "current_version": "1.7.1",
      "min_version": "1.6.0",
      "download_url": "https://filesorter.app/download",
      "changelog": [
        "Improved Linux wrapper backend selection"
      ]
    }
  }
}
```

Compatibility note:

- Older app versions only read the flat top-level fields under `update`, so keep `current_version`, `min_version`, and `download_url` there as a legacy compatibility stream if you still need to support them.
- Newer app versions prefer the platform-specific streams and will use `update.windows`, `update.macos`, or `update.linux` when present.
- The legacy compatibility stream can only represent one generic stream, not separate per-platform versions or installers.
- `changelog` is evaluated per stream. Use a JSON array of strings for new feeds; each entry is shown as a bullet item in the update dialog for that stream.

Windows-only direct installer updates:

- `installer_url` - direct URL to the Windows installer package.
- `installer_sha256` - SHA-256 checksum used to verify the downloaded installer before launch.
- `installer_url` can now also point to a ZIP archive, as long as the archive contains exactly one installer payload (`.exe` or `.msi`).
- When both fields are present on Windows, the app can download the installer, verify it, and then prompt: `Quit the app and launch the installer to update`.

Development feed selection:

- When the app starts with `--development`, the updater prefers `UPDATE_SPEC_FILE_URL_DEVELOPMENT`.
- If `UPDATE_SPEC_FILE_URL_DEVELOPMENT` is unset, development mode falls back to `UPDATE_SPEC_FILE_URL`.

GUI test mode:

- `--test` launches the normal app window, implies development mode, and adds a **Tests** menu.
- The first test preset is **Run large whitelist LLM test…**. It creates a sample folder, configures a large transient category whitelist, and starts the normal analysis flow with the currently selected real LLM.
- The large-whitelist preset is meant for manual/runtime validation: inspect the Review dialog to see whether the real LLM selected the expected broad categories from the compact whitelist candidates.
- Test mode uses the user's selected LLM configuration but stores test-mode whitelists, categorization cache, learned behavior, undo data, and sample files under an isolated `test_mode_profile` directory inside the normal config directory.
- Test mode does not save normal app settings on shutdown, so the preset folder/whitelist selection should not replace the user's ordinary configuration.

Headless self-test mode:

- `--self-test` runs deterministic self-tests from the production executable and exits with a pass/fail status instead of opening the main window.
- `--self-test=whitelist` runs the deterministic large-whitelist suite explicitly. `--self-test=whitelists` is accepted as an alias.
- The headless whitelist suite uses temporary app data, a large synthetic category list, learned-behavior fixtures, and a deterministic LLM stub. It verifies that large whitelists are reduced to relevant candidates, learned categories can outrank generic model output, and Unicode labels such as emoji survive the flow.
- The full Catch2 unit suite extends this coverage with smart branching whitelist persistence, prompt construction, and valid category/subcategory pair enforcement.
- On Windows GUI builds, add `--console-log` if you want to see the self-test output in the launching console.

Windows updater live-test mode:

- `aifilesorter.exe` accepts the following flags directly on Windows:
  `--updater-live-test`
  `--updater-live-test-url=<https://.../AIFileSorterSetup.zip>`
  `--updater-live-test-sha256=<sha256-of-the-downloaded-package>`
  `--updater-live-test-version=<optional-version>`
  `--updater-live-test-min-version=<optional-min-version>`
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

## Categorization cache and learned behavior

AI File Sorter keeps two separate kinds of local memory under the app config directory (the base directory can be overridden via `AI_FILE_SORTER_CONFIG_DIR`):

- A **categorization cache** for faster reruns and consistency hints.
- A separate **learned-behavior database** for category decisions you explicitly approve in the Review dialog.

### Categorization cache

AI File Sorter stores categorization results in a local SQLite database next to `config.ini`. This cache allows the app to skip already-processed files, preserve rename suggestions between runs, and reuse recent category/subcategory assignments as consistency hints.

What is stored:

- Directory path, file name, and file type (used as a unique key).
- Category/subcategory, taxonomy id, categorization style, and timestamp.
- Suggested filename (for picture and document rename suggestions).
- Rename-only flag (used when picture/document rename-only modes are enabled).
- Rename-applied flag (marks when a rename was executed so it is not offered again).

This cache is used as lightweight memory for consistency, not as model training. In **More consistent** mode, the app can feed recent assignments for similar file types back into the prompt so labels trend toward the same taxonomy over time.

If you rename or move a file from the Review dialog, the cache entry is updated to the new name. Already-renamed picture files are skipped for visual analysis and rename suggestions on later runs. In the Review dialog, those already-renamed rows are hidden when rename-only is enabled, but they stay visible when categorization is enabled so you can still move them into category folders. To reset a folder's cache, accept the recategorization prompt. You can also delete the cache file directly (or point `CATEGORIZATION_CACHE_FILE` to a new filename).

### Local learning from approved reviews

When you approve categories in the Review dialog, the app can remember those local decisions and reuse them as hints for future runs. This helps stabilize similar folders over time, but it does **not** train or modify the underlying AI model.

These learned examples are stored in a separate local database from the normal categorization cache. Clearing the categorization cache does **not** remove learned behavior.

To remove learned review data, use **Settings → Reset learned behavior…**.

### Cache maintenance tooling

Use **Settings → Clear cache…** to inspect and clear the disposable maintenance data the app manages:

- **Categorization cache**: Past file and folder categorization results.
- **Image location cache**: Reverse-geocoded place names for photo GPS lookups.
- **Logs**: Application log files used for troubleshooting and diagnostics.

Downloaded models are managed separately in **Settings → Select LLM…** and are not removed by the cache cleanup dialog.

---

## Uninstallation

- **Debian/Ubuntu package installs**: `sudo apt remove aifilesorter`
- **Linux source installs**: `cd app && sudo make uninstall`
- **macOS source installs**: `cd app && sudo make uninstall`

For source installs, `make uninstall` removes the executable and the staged precompiled libraries. You can also delete downloaded local LLM models in `~/.local/share/aifilesorter/llms` (Linux) or `~/Library/Application Support/aifilesorter/llms` (macOS) if you no longer need them.

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

## Using a custom OpenAI-compatible API

Prefer an OpenAI-compatible endpoint such as **LM Studio**, **Ollama**, or your own hosted gateway? AI File Sorter can use that too:

1. Open **Settings -> Select LLM** in the app.
2. Choose **Custom OpenAI-compatible API (advanced)**.
3. Click **Add…**, then enter a friendly name, the endpoint base URL, the model name to use, and an API key if your endpoint requires one.
4. Save the endpoint, select it from the list, and click **OK**.
5. The endpoint configuration is stored locally in your AI File Sorter config and can be edited or removed later from the same dialog.

Use this option for local servers or remote providers that follow the OpenAI-style API shape. Response time can be tuned with `AI_FILE_SORTER_CUSTOM_LLM_TIMEOUT`; rate-limited providers can be paced with `AI_FILE_SORTER_REMOTE_REQUESTS_PER_MINUTE` or `[Settings] RemoteRequestsPerMinute` (see [Environment variables](#environment-variables)).

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

For log locations, rotation details, and common troubleshooting notes, see [TROUBLESHOOTING.md](TROUBLESHOOTING.md).

---

## Help and onboarding

If you want an in-app walkthrough before your first run, open **Help → Quick Start Guide**. The Quick Start guide is localized and covers a safe small-folder trial, the review flow, undo, local learning, and the most common settings you may want to change.

If something looks wrong or you want troubleshooting tips, open **Help → FAQ**.

For log locations, rotation details, and other troubleshooting notes outside the app, see [TROUBLESHOOTING.md](TROUBLESHOOTING.md).

---

## How to Use

1. Launch the application (see the last step in [Installation](#installation) according your OS).
2. Select a directory to analyze.

If you want a guided walkthrough first, open **Help → Quick Start Guide**. For troubleshooting during setup or after a run, open **Help → FAQ**.

### Using dry run and undo

- In the results dialog, you can enable **"Dry run (preview only, do not move files)"** to preview planned moves. A preview dialog shows From/To without moving any files.
- After a real sort, the app saves a persistent undo plan. You can revert later via **Edit → "Undo last run"** (best-effort; skips conflicts/changes).

3. Tick off the checkboxes on the main window according to your preferences.
4. Click the **"Analyze"** button. The app will scan each file and/or directory based on your selected options.

Recursive scans intentionally skip recognized structured project roots, including Unity, Unreal, Godot, conservative Blender project folders, Git repositories, and common source-code project layouts. This protects folders where moving individual files can break project-relative links, imports, metadata, or build files.

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

Support the development of **AI File Sorter** and its future features. Every contribution counts, and the app remains usable without removing privacy, preview, undo, or local-processing features.

Suggested support levels:

- **$5**: Say thanks and help keep the project going.
- **$15**: Support ongoing fixes, packaging work, and compatibility testing.
- **$30**: Help fund release maintenance, signing/listing costs, and user support.
- **$100+**: Sponsor heavier development work or commercial/organizational use.

You can still choose any amount from **$1 and up**. Donation codes are supporter markers that hide the periodic support reminder; they are not used to take away the free local core.

- **[Donate](https://filesorter.app/donate/)**

---
