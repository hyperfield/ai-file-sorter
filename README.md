<!-- markdownlint-disable MD046 -->
# AI File Sorter

[![Version](https://badgen.net/badge/version/0.9.2/green)](#) [![Donate via PayPal](https://badgen.net/badge/donate/PayPal/blue)](https://paypal.me/aifilesorter)

AI File Sorter is a powerful, cross-platform desktop application that automates file organization. Featuring AI integration and a user-friendly GTK-based interface, it categorizes and sorts files and folders based on their names and extensions. The app intelligently assigns categories and, optionally, subcategories, which you can review and edit before confirming. Once approved, the necessary folders are created, and your files are sorted accordingly. The app uses local (LLaMa, Mistral) and remote (ChatGPT 4o-mini) LLMs for this task, depending on your choice.

[![Download ai-file-sorter](https://a.fsdn.com/con/app/sf-download-button)](https://sourceforge.net/projects/ai-file-sorter/files/latest/download)

![AI File Sorter Screenshot](screenshots/AI-File-Sorter_screenshot-1.png) ![AI File Sorter Screenshot](screenshots/AI-File-Sorter_screenshot-2.png) ![AI File Sorter Screenshot](screenshots/AI-File-Sorter_screenshot-3.png) ![AI File Sorter Screenshot](screenshots/AI-File-Sorter_screenshot-4.png)

---

- [AI File Sorter](#ai-file-sorter)
  - [Changelog](#changelog)
    - [\[0.9.2\] - 2025-08-06](#092---2025-08-06)
    - [\[0.9.1\] - 2025-08-01](#091---2025-08-01)
    - [\[0.9.0\] - 2025-07-18](#090---2025-07-18)
  - [Features](#features)
  - [Requirements](#requirements)
  - [Installation](#installation)
    - [Windows](#windows)
      - [Install Git](#install-git)
        - [Clone the repository](#clone-the-repository)
        - [Navigate into the directory](#navigate-into-the-directory)
      - [Compile the app](#compile-the-app)
    - [MacOS](#macos)
        - [Clone the repository](#clone-the-repository-1)
        - [Navigate into the directory](#navigate-into-the-directory-1)
      - [Compile the app](#compile-the-app-1)
  - [Uninstallation](#uninstallation)
    - [Linux](#linux)
        - [Clone the repository](#clone-the-repository-2)
        - [Navigate into the directory](#navigate-into-the-directory-2)
      - [Compile the app](#compile-the-app-2)
      - [1. Install the dependencies](#1-install-the-dependencies)
        - [Debian / Ubuntu](#debian--ubuntu)
        - [Fedora / RedHat](#fedora--redhat)
        - [Arch / Manjaro](#arch--manjaro)
      - [2. Compile `llama.cpp`](#2-compile-llamacpp)
        - [Debian / Ubuntu](#debian--ubuntu-1)
        - [Fedora / RedHat](#fedora--redhat-1)
        - [Arch / Manjaro](#arch--manjaro-1)
        - [All Linux](#all-linux)
      - [3. \[Optional\] Get API key](#3-optional-get-api-key)
      - [4. Compile and install AI File Sorter](#4-compile-and-install-ai-file-sorter)
  - [API Key, Obfuscation, and Encryption](#api-key-obfuscation-and-encryption)
  - [Uninstallation](#uninstallation-1)
  - [How to Use](#how-to-use)
  - [Sorting a Remote Directory (e.g., NAS)](#sorting-a-remote-directory-eg-nas)
  - [Contributing](#contributing)
  - [Credits](#credits)
  - [License](#license)
  - [Donation](#donation)

---

## Changelog

### [0.9.2] - 2025-08-06

  - Bug fixes.
  - Increased code coverage with logging.

### [0.9.1] - 2025-08-01

  - Bug fixes.
  - Minor improvements for stability.
  - OpenCL is now not supported by default in the build script for llama.cpp.

### [0.9.0] - 2025-07-18

  - Local LLM support with `llama.cpp`.
  - LLM selection and download dialog.
  - Improved `Makefile` for a more hassle-free build and installation.
  - Minor bug fixes and improvements.

---

## Features

- **AI-Powered Categorization**: Classify files intelligently using either a **local LLM** (LLaMa, Mistral) or a
                                 remote LLM (ChatGPT), depending on your preference.
- **Offline-Friendly**: Use a local LLM to categorize files entirely - no internet or API key required.
  **Customizable Sorting Rules**: Automatically assign categories and subcategories for granular organization.
- **Intuitive Interface**: Lightweight and user-friendly for fast, efficient use.
- **Cross-Platform Compatibility**: Works on Windows, macOS, and Linux.
- **Local Database Caching**: Speeds up repeated categorization and minimizes remote LLM usage costs.
- **Sorting Preview**: See how files will be organized before confirming changes.
- **Secure API Key Encryption**: When using the remote model, your API key is stored securely with encryption.
- **Update Notifications**: Get notified about updates - with optional or required update flows.

---

## Requirements

- **Operating System**: Windows, macOS, or Linux with a stable internet connection.
- **C++ Compiler**: A recent `g++` version (used in `Makefile`).
- **Platform specific requirements**:
    - **Windows**: `MSYS2` / `MINGW64` with some requirements.
    - **MacOS**: `brew` to install some requirements. `Xcode`.
  
  Optional:
    - **Git**: For cloning this repository. You can alternatively download the repo in a zip archive.
    - **OpenAI API Key**: Not needed for local LLMs.

---

## Installation

File categorization with local LLMs is completely free of charge.

If you want file categorization with ChatGPT, you will need to get an OpenAI API key and add a minimal balance to it for this program to work. Categorization is quite cheap, so $0.01 will be enough to categorize a relatively large number of files. The instructions on how to integrate your API key into the app are given below. You can also download a [Release](https://github.com/hyperfield/ai-file-sorter/releases) version, which has an embedded API key.

### Windows

#### Install Git

First, you need `git`. Download `git-scm` for Windows from [git-scm.com](https://git-scm.com/downloads) and install it. Verify the installation in `cmd` or `powershell` with

    git --version

You can also now launch `Git Bash` from Start Menu.

##### Clone the repository

    git clone https://github.com/hyperfield/ai-file-sorter.git
    cd ai-file-sorter
    git submodule update --init --recursive --remote

##### Navigate into the directory

    cd ai-file-sorter

#### Compile the app

**Important**: Install all the packages specified below in their default installation directories. Otherwise, you'll need to adjust the paths in `build_llama_windows.ps1` and `Makefile`. 

1. Install [MSYS2](https://www.msys2.org/). Make sure to launch *as Administrator* `MSYS2 MINGW64`, **NOT** `MSYS2 MSYS`. If you don't launch it as Administrator, `make install` won't work. You may run it as regular user if you don't need to use `make install` in the last step.
2. Update MSYS2 packages: `pacman -Syu`.
3. Install dependencies:
   
```bash
pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-gtk3 mingw-w64-x86_64-gtkmm3 mingw-w64-x86_64-jsoncpp mingw-w64-x86_64-pcre mingw-w64-x86_64-libidn2 mingw-w64-x86_64-libssh mingw-w64-x86_64-libpsl mingw-w64-x86_64-openldap mingw-w64-x86_64-gnutls mingw-w64-x86_64-lz4 mingw-w64-x86_64-libgcrypt mingw-w64-x86_64-fmt mingw-w64-x86_64-spdlog mingw-w64-x86_64-curl mingw-w64-x86_64-openblas make
```

4. Install [Visual Studio Community](https://visualstudio.microsoft.com/vs/community/).
   **Imporant**: Be sure to check the `Dekstop development with C++` workflow, under which the `Windows 1x SDK` (depending on your Windows version) should be checked.

5. **Optional**: If you want to compile AI File Sorter with CUDA (for Nvidia GPUs only) then download and install [CUDA Toolkit](https://developer.nvidia.com/cuda-downloads). CUDA significantly increases the compute speed of local LLMs. Otherwise, AI File Sorter will fall back on either OpenCL or OpenBLAS (CPU only, but faster than without it).

6. Launch `Developer PowerShell for VS 2022` and `cd path\to\cloned\github\repo\app\scripts` (e.g., `cd C:\Users\username\repos\ai-file-sorter\app\scripts`). Check that the script `build_llama.windows.ps1` contains the paths in accordance with your versions of CUDA and Visual Studio tools. In particular, check the version numbers in these lines:

    ```
    $cudaRoot = "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.9"
    -DCMAKE_C_COMPILER="C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe" `
    -DCMAKE_CXX_COMPILER="C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe"
    ```

    Save any changes.

7. In `Developer PowerShell for VS 2022`, run

    **If you have CUDA**:

        powershell -ExecutionPolicy Bypass -File .\build_llama_windows.ps1 cuda=on

    **If you don't have CUDA**:

        powershell -ExecutionPolicy Bypass -File .\build_llama_windows.ps1 cuda=off

    `llama.cpp` will be built from source.

8. **Optional** (not needed if you want to use only local LLMs for file sorting). Go to [API Key, Obfuscation, and Encryption](#api-key-obfuscation-and-encryption) and complete all steps there before proceeding to step 6 here.

9. Go back the `MSYS2 MINGW64` shell (ensure you ran it *as Administrator*, otherwise `make install` won't work). Go to the cloned repo's `app/resources` path (e.g., `/c/Users/username/repos/ai-file-sorter/app/resources`) and run `bash compile-resources.sh`. Go to the `app` directory (`cd ..`).

10. Run `make`, `make install` and `make clean`. The executable `AiFileSorter.exe` will be located in `C:\Program Files\AiFileSorter`. You can add the directory to `%PATH%`.

To uninstall, launch `MSYS2 MINGW64` (**NOT** `MSYS2 MSYS`) *as Administrator*, go to the same directory (`ai-file-sorter/app`) and issue the command `make uninstall`.

---

### MacOS

##### Clone the repository

    git clone https://github.com/hyperfield/ai-file-sorter.git
    cd ai-file-sorter
    git submodule update --init --recursive --remote

##### Navigate into the directory

    cd ai-file-sorter

#### Compile the app

1. Install Xcode (required for Accelerate.framework and AppleClang):
- From the App Store, install **Xcode**.
- Then run in Terminal:

      sudo xcode-select -s /Applications/Xcode.app/Contents/Developer

2. Install Homebrew:

       /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

3. Install dependencies:
   ```bash
   brew install cmake gcc atkmm@2.28 cairo at-spi2-core pangomm@2.46 gtk+3 gtkmm3 glibmm@2.66 cairomm@1.14 pango harfbuzz glib gettext curl jsoncpp sqlite3 openssl@3 pkg-config libffi expat xorgproto fmt spdlog adwaita-icon-theme hicolor-icon-theme

   brew install --cask font-0xproto
   ```

4. Go to [API Key, Obfuscation, and Encryption](#api-key-obfuscation-and-encryption) and complete all steps there before proceeding to step 6 here.

5. Go to `app/resources` and run `./compile-resources.sh`. Go back to the `app` directory.

6. Run `make`, `sudo make install`, `make clean`. Then you can launch the app with the command `aifilesorter`.

---

## Uninstallation

Use `sudo make uninstall` in the same `app` subdirectory

---

### Linux

##### Clone the repository

```bash
git clone https://github.com/hyperfield/ai-file-sorter.git
cd ai-file-sorter
git submodule update --init --recursive --remote
```

##### Navigate into the directory

```bash
cd ai-file-sorter
```

#### Compile the app

#### 1. Install the dependencies

##### Debian / Ubuntu

```bash
sudo apt update && sudo apt install -y build-essential cmake libgtk-3-dev libgtkmm-3.0-dev libgdk-pixbuf2.0-dev libglib2.0-dev libcurl4-openssl-dev libjsoncpp-dev libsqlite3-dev libssl-dev libx11-dev libxi-dev libxfixes-dev libcairo2-dev libatk1.0-dev libepoxy-dev libharfbuzz-dev libfontconfig1-dev libfmt-dev libspdlog-dev libpng-dev libjpeg-dev libffi-dev libpcre3-dev libnghttp2-dev libidn2-0-dev librtmp-dev libssh-dev libpsl-dev libkrb5-dev libldap2-dev libbrotli-dev libxcb1-dev libxrandr-dev libxinerama-dev libxkbcommon-dev libwayland-dev libthai-dev libfreetype6-dev libgraphite2-dev libgnutls28-dev libp11-kit-dev liblzma-dev liblz4-dev libgcrypt20-dev libsystemd-dev ocl-icd-opencl-dev
```

##### Fedora / RedHat

```bash
sudo dnf install -y gcc-c++ make cmake gtk3-devel gtkmm30-devel gdk-pixbuf2-devel glib2-devel   libcurl-devel jsoncpp-devel sqlite-devel openssl-devel libX11-devel libXi-devel libXfixes-devel cairo-devel atk-devel epoxy-devel harfbuzz-devel fontconfig-devel fmt-devel spdlog-devel libpng-devel libjpeg-turbo-devel libffi-devel pcre-devel libnghttp2-devel libidn2-devel librtmp-devel libssh-devel libpsl-devel krb5-devel openldap-devel brotli-devel xcb-util-devel libXrandr-devel libXinerama-devel xkbcommon-devel wayland-devel libthai-devel freetype-devel graphite2-devel gnutls-devel p11-kit-devel xz-devel lz4-devel libgcrypt-devel systemd-devel ocl-icd-devel
```

##### Arch / Manjaro

```bash
sudo pacman -Syu --needed base-devel cmake gtk3 gtkmm3 gdk-pixbuf2 glib2 curl jsoncpp sqlite openssl libx11 libxi libxfixes cairo atk epoxy harfbuzz fontconfig fmt spdlog libpng libjpeg-turbo libffi pcre nghttp2 libidn2 rtmpdump libssh libpsl krb5 openldap brotli libxcb libxrandr libxinerama xkbcommon wayland libthai freetype2 graphite gnutls p11-kit xz lz4 libgcrypt systemd opencl-headers ocl-icd
```

#### 2. Compile `llama.cpp`

##### Debian / Ubuntu

- If you are going to compile `llama.cpp` with CUDA (you have an Nvidia GPU), you need `g++` of at least version 10 and CUDA Toolkit:

  ```bash
  sudo apt install g++-10 nvidia-cuda-toolkit
  ```

##### Fedora / RedHat

- If you are going to compile `llama.cpp` with CUDA (you have an Nvidia GPU), you need `g++` of at least version 10  and CUDA Toolkit:

  ```bash
  sudo dnf install gcc-toolset-10
  ```

  Then enable it with:

  ```bash
  scl enable gcc-toolset-10 bash
  ```

  Download CUDA from [Nvidia](https://developer.nvidia.com/cuda-downloads?target_os=Linux) and install it.

##### Arch / Manjaro

- If you are going to compile `llama.cpp` with CUDA, you need CUDA Toolkit:

  ```bash
  sudo pacman -S cuda
  ```

##### All Linux

- If you have CUDA, run

  ```bash
  scripts/build_llama_linux.sh cuda=on
  ```

- If you don't have CUDA, run

  ```bash
  scripts/build_llama_linux.sh
  ```

#### 3. [Optional] Get API key

*Only needed if you want to be able to use the Remote LLM (ChatGPT o4-mini)*.

Go to the section [API Key, Obfuscation, and Encryption](#api-key-obfuscation-and-encryption) and complete all steps there before proceeding to step 4 here.

#### 4. Compile and install AI File Sorter

While still in `ai-file-sorter/app`, run

```bash
make
sudo make install
```

Run the app with

```bash
aifilesorter
```

---

## API Key, Obfuscation, and Encryption

**Important**: This step is needed *only* if you are going to use the Remote LLM option.

Before compiling the app:

1. Get an OpenAI API key from the [OpenAI website](https://platform.openai.com/).  
   A minimal balance is required in your OpenAI API account for the app to function.

2. Generate a 32-character random secret key, e.g., using [this tool](https://passwords-generator.org/32-character).

    **Important**: If you're compiling on Windows, make sure there is NO `=` in the generated key! If one or more `=` are there, regenerate the key!
    **Important**: If you're compiling on Windows, it's probably best to avoid symbols due to possible unpredictable parsing issues.

    Your secret key could look something like `sVPV2fWoRg5q62AuCGVQ4p0NbHIU5DEv` or `du)]--Wg#+Au89Ro6eRMJc"]qx~owL_X`.

3. Navigate to the `api-key-encryption` folder, then make a file named `encryption.ini` with the following content:

    ```
    LLM_API_KEY=sk-...
    SECRET_KEY=your-generated-32-byte-secret-key
    ```

4. Run the `compile.sh` script in the same directory to generate the executable `obfuscate_encrypt`.
 due 
5. Execute `obfuscate_encrypt` to generate:
   - Obfuscated Key part 1
   - Obfuscated Key part 2
   - Encrypted data (hex)

6. Update the application files:
   - Update `app/include/CryptoManager.hpp` with Obfuscated Key part 1:
     ```cpp
     static constexpr char embedded_pc[] = "insert-obfuscated-Key-part-1-here";
     ```
   - Add the values to `app/resources/.env` as shown:
     ```
     ENV_PC=obfuscated-key-part2-value
     ENV_RR=encrypted-data-hex-value
     ```

7. Continue with [Installation](#installation)
---


## Uninstallation

In the same subdirectory `app`, run `sudo make uninstall`.

---

## How to Use

1. Launch the application (see the last step in [Installation](#installation) according your OS).
2. Select a directory to analyze.
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

- Curl: https://github.com/curl/curl
- Gtk+3: https://gitlab.gnome.org/GNOME/gtk
- Dotenv: https://github.com/motdotla/dotenv
- git-scm: https://git-scm.com
- Hugging Face: https://huggingface.co
- JSONCPP: https://github.com/open-source-parsers/jsoncpp
- LLaMa: https://www.llama.com
- llama.cpp https://github.com/ggml-org/llama.cpp
- Minstral AI: https://mistral.ai
- MSYS2: https://www.msys2.org
- OpenAI: https://platform.openai.com/docs/overview
- OpenSSL: https://github.com/openssl/openssl
- SoftPedia: https://www.softpedia.com/get/File-managers/AI-File-Sorter.shtml
- spdlog: https://github.com/gabime/spdlog

## License

This project is licensed under the GNU AFFERO GENERAL PUBLIC LICENSE (GNU AGPL). See the [LICENSE](LICENSE) file for details.

---

## Donation

Support the development of **AI File Sorter** and its future features. Every contribution counts!

- **[Donate via PayPal](https://paypal.me/aifilesorter)**
- **Bitcoin**: 12H8VvRG9PGyHoBzbYxVGcu8PaLL6pc3NM
- **Ethereum**: 0x09c6918160e2AA2b57BfD40BCF2A4BD61B38B2F9
- **Tron**: TGPr8b5RxC5JEaZXkzeGVxq7hExEAi7Yaj

USDT is also accepted in Ethereum and Tron chains.

---