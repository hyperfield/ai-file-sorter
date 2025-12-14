# AI File Sorter - Replit Project

## Overview

AI File Sorter is a cross-platform C++20 Qt6 desktop application that automates file organization using AI-powered categorization via local LLMs (llama.cpp). It helps tidy up cluttered folders by intelligently assigning categories based on filenames, extensions, and context.

## Environment Status

**This project cannot be built or run in the Replit environment** due to fundamental platform constraints:

1. **Path length limitations**: CMake encounters MAX_PATH errors with deep nix store paths
2. **Git submodule restrictions**: The llama.cpp submodule cannot be initialized
3. **Desktop GUI requirements**: Qt6 with OpenGL is a native desktop GUI toolkit

## Build Requirements

- C++20 compiler (g++ or clang++)
- Qt6 (Core, Gui, Widgets) with OpenGL support
- llama.cpp library (built from source via submodule)
- Native libraries: curl, sqlite3, openssl, fmt, spdlog, jsoncpp, gettext

## Local Development

### Linux

```bash
sudo apt install build-essential cmake git qt6-base-dev qt6-base-dev-tools \
  libcurl4-openssl-dev libjsoncpp-dev libsqlite3-dev libssl-dev libfmt-dev libspdlog-dev
git clone https://github.com/hyperfield/ai-file-sorter.git
cd ai-file-sorter
git submodule update --init --recursive
./app/scripts/build_llama_linux.sh cuda=off vulkan=off
cd app && make -j4
```

### macOS

```bash
brew install qt curl jsoncpp sqlite openssl fmt spdlog cmake git
git clone https://github.com/hyperfield/ai-file-sorter.git
cd ai-file-sorter
git submodule update --init --recursive
./app/scripts/build_llama_macos.sh
cd app && make -j4
```

### Windows

Uses CMake + vcpkg. See README.md for detailed Windows build instructions.

## Project Structure

```
app/
  CMakeLists.txt       - CMake build configuration
  Makefile             - Alternative make-based build (Linux/macOS)
  main.cpp             - Application entry point
  include/             - Header files (43 headers)
    external/
      llama.cpp/       - LLM inference engine (git submodule)
  lib/                 - Source files (32 C++ files)
  resources/           - UI resources, icons, translations, certificates
  scripts/             - Build and utility scripts
external/
  Catch2/              - Test framework (git submodule)
tests/
  unit/                - Unit tests
```

## Key Features

- **AI-Powered Categorization**: Local LLMs (LLaMa, Mistral) or ChatGPT API
- **Offline-Friendly**: No internet required with local models
- **Taxonomy-Based System**: Builds smart internal reference for file patterns
- **Two Categorization Modes**: Refined (detailed) or Consistent (uniform)
- **Category Whitelists**: Constrain model output for specific projects
- **Dry Run / Preview**: Preview moves before execution
- **Persistent Undo**: Revert changes even after closing dialogs
- **Multi-Language UI**: English, German, French, Spanish, Italian, Turkish

## Architecture Notes

- Uses Qt6 Widgets for cross-platform GUI
- SQLite for local caching and database
- llama.cpp for local LLM inference (Metal on macOS, CUDA/Vulkan/CPU on others)
- libcurl for HTTP requests (ChatGPT API, updates)
- spdlog/fmt for logging

## Running This Workflow

The configured workflow (`bash run_info.sh`) displays project information and build instructions since the application cannot be compiled in this environment.
