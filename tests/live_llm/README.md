# Headless Live LLM Tests

This directory contains optional black-box integration tests that run the built
`aifilesorter` executable in headless mode against real files and real local
LLM model artifacts.

These tests are intentionally not part of the default suite. They are slow,
hardware-dependent, and partly nondeterministic because model wording varies.
They assert the production contract and stable invariants instead: valid status
JSON, review-plan generation, non-empty categories, whitelist confinement,
selected-file boundaries, extension preservation, and localized rename output.

## Requirements

- A built `aifilesorter` executable.
- A local text GGUF model for document rename and categorization, either passed
  to the runner or selected in the normal AI File Sorter settings.
- Optional visual GGUF plus matching mmproj GGUF for image-content rename cases.
- Network access for first-run public fixture downloads, unless the fixture
  cache is already populated.

## CTest

Configure with the live test option and the normal test option.

Linux/macOS:

```bash
cmake -S app -B build-tests -DAI_FILE_SORTER_BUILD_TESTS=ON -DAI_FILE_SORTER_ENABLE_LIVE_LLM_TESTS=ON
cmake --build build-tests --parallel "$(nproc)"
```

Windows recommended helper path:

```powershell
.\app\build_windows.ps1 -Configuration Release -Variants Standard -BuildTests -EnableLiveLlmTests
```

From `cmd.exe`, use `app\build_windows.cmd -Configuration Release -Variants Standard -BuildTests -EnableLiveLlmTests`.

Windows manual CMake path, from an x64 Visual Studio Developer PowerShell with
Qt and vcpkg configured:

```powershell
$env:VCPKG_ROOT="D:\path\to\vcpkg"
$qt="C:\Qt\6.6.3\msvc2019_64"
$toolchain=Join-Path $env:VCPKG_ROOT "scripts\buildsystems\vcpkg.cmake"
cmake -S app -B build-tests -G "Ninja" `
  -DCMAKE_PREFIX_PATH=$qt `
  "-DCMAKE_TOOLCHAIN_FILE=$toolchain" `
  -DVCPKG_MANIFEST_DIR=app `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DAI_FILE_SORTER_BUILD_TESTS=ON `
  -DAI_FILE_SORTER_ENABLE_LIVE_LLM_TESTS=ON
cmake --build build-tests --config Release --parallel $env:NUMBER_OF_PROCESSORS
```

Use `cmake --fresh` when supported, delete/recreate the build directory, or use
a new build directory when reusing a tree that was previously configured with
another generator or Visual Studio instance. CMake stores the generator instance
in `CMakeCache.txt`, so a stale cache can keep pointing at a removed Visual
Studio installation even when the repo does not hardcode that path.

Set a model path before running the `live-llm` label, or omit this step if the
normal AI File Sorter settings already select a local/custom GGUF model:

```powershell
$env:AI_FILE_SORTER_LIVE_LLM_MODEL="C:\models\text-model.gguf"
$env:AI_FILE_SORTER_LIVE_BACKEND="cuda"  # optional: auto, cpu, cuda, or vulkan
ctest --test-dir build-tests -L live-llm --output-on-failure
```

If you configured via `app\build_windows.ps1`, use `ctest --test-dir app\build-windows -C Release -L live-llm --output-on-failure`.
For a manual Windows CMake build, use `ctest --test-dir build-tests -C Release -L live-llm --output-on-failure`.
Bundled Windows builds run through `aifilesorter.exe` so llama.cpp runtime DLL
selection follows the same CUDA -> Vulkan -> CPU logic as the normal non-Store
app. If an older CTest registration still passes `aifilesorter-bin.exe`, the
runner automatically uses the sibling launcher when it exists.
Use `AI_FILE_SORTER_LIVE_BACKEND=cuda` to validate CUDA specifically, leave it
unset or set it to `auto` to test normal launcher auto-selection, and use `cpu`
only for deterministic CPU/OpenBLAS runs.

CTest only streams passing-test output when verbose mode is enabled:

```powershell
ctest --test-dir app\build-windows -C Release -L live-llm -V
```

Verbose progress includes the backend environment received by the headless text
LLM client, for example `Local text LLM backend request:
AI_FILE_SORTER_GPU_BACKEND=cuda`. If that line says `cpu`, clear any stale
`AI_FILE_SORTER_LIVE_BACKEND=cpu` setting and rebuild after stopping old test
processes that may still hold `aifilesorter-bin.exe`.

For a non-verbose CTest run, tail the runner's progress log from another shell:

```powershell
$work = Get-Content "$env:TEMP\aifs-live-llm-latest.txt"
Get-Content (Join-Path $work "progress.log") -Wait
```

Image rename cases are skipped unless both visual artifacts are configured
explicitly or a custom visual model pair is selected in app settings:

```powershell
$env:AI_FILE_SORTER_LIVE_VISUAL_MODEL="C:\models\vision-model.gguf"
$env:AI_FILE_SORTER_LIVE_VISUAL_MMPROJ="C:\models\mmproj-model.gguf"
ctest --test-dir build-tests -L live-llm --output-on-failure
```

If the executable is missing, no explicit text model is provided, and no usable
local GGUF can be resolved from app settings, the runner exits with `77` so
CTest reports the suite as skipped.

## Direct Runner

```powershell
python tests\live_llm\headless_live_llm_tests.py `
  --app app\build-windows\Release\aifilesorter.exe `
  --model C:\models\text-model.gguf `
  --backend cpu `
  --keep-work-dir `
  --verbose
```

Useful filters:

```powershell
python tests\live_llm\headless_live_llm_tests.py --app app\build-windows\Release\aifilesorter.exe --model C:\models\text-model.gguf --backend cpu --only rename_documents
python tests\live_llm\headless_live_llm_tests.py --app app\build-windows\Release\aifilesorter.exe --model C:\models\text-model.gguf --backend cpu --only whitelist
```

## Environment Variables

- `AI_FILE_SORTER_LIVE_APP`: executable path when running the script directly.
- `AI_FILE_SORTER_LIVE_LLM_MODEL`: text GGUF path. If unset, the runner falls
  back to the selected local model in AI File Sorter `config.ini`.
- `AI_FILE_SORTER_LIVE_BACKEND`: Windows launcher backend selection. Use
  `auto`, `cpu`, `cuda`, or `vulkan`; `cuda` validates the CUDA path explicitly
  and `cpu` is the most deterministic for CI-like runs.
- `AI_FILE_SORTER_LIVE_SETTINGS_FILE`: optional explicit AI File Sorter
  `config.ini` path for model fallback.
- `AI_FILE_SORTER_LIVE_VISUAL_MODEL`: optional image model GGUF path.
- `AI_FILE_SORTER_LIVE_VISUAL_MMPROJ`: optional image mmproj GGUF path.
- `AI_FILE_SORTER_LIVE_FIXTURE_CACHE`: public fixture cache directory.
- `AI_FILE_SORTER_LIVE_WORK_DIR`: generated work/config/log directory.
- `AI_FILE_SORTER_LIVE_TIMEOUT`: per-headless-command timeout in seconds.
- `AI_FILE_SORTER_LIVE_ONLY`: regex filter for case names.
- `AI_FILE_SORTER_LIVE_SKIP_DOWNLOADS`: skip public downloads.
- `AI_FILE_SORTER_LIVE_REQUIRE_DOWNLOADS`: fail if public downloads fail.
- `AI_FILE_SORTER_LIVE_FORCE_VISUAL_CPU`: set `AI_FILE_SORTER_VISUAL_USE_GPU=0`.
- `AI_FILE_SORTER_LIVE_KEEP_WORK_DIR`: keep generated fixtures and logs. Failing
  runs are preserved automatically even when this is unset.

Each run writes `progress.log` at the work-dir root and stores the latest work
directory path in `%TEMP%\aifs-live-llm-latest.txt`. Per-case folders under
`runs\` include `command.txt`, `stdout.txt`, `stderr.txt`, `status.json`, and any
review JSON emitted by the app.

## Current Coverage

- Categorization without subcategories.
- Categorization with subcategories.
- Same-folder selected-file boundary for headless auto-apply.
- Whitelist-restricted categorization using an isolated persisted whitelist.
- Document rename in English, French, Simplified Chinese, and Hindi.
- Image-content rename in English, French, Simplified Chinese, and Hindi when
  visual model artifacts are supplied.
- Audio metadata rename from a generated WAV file with RIFF INFO tags.
- Categorize-and-rename review-plan generation.

## Fixture Sources

The runner generates text, Markdown, CSV, YAML, JSON, XML, HTML, RTF, and WAV
fixtures locally. It also downloads small public fixtures into a cache:

- W3C dummy PDF: `https://www.w3.org/WAI/ER/tests/xhtml/testfiles/resources/pdf/dummy.pdf`
- Wikimedia PNG: `https://commons.wikimedia.org/wiki/File:PNG_transparency_demonstration_1.png`
- Wikimedia JPEG: `https://commons.wikimedia.org/wiki/File:JPEG_example_flower.jpg`
- Wikimedia GIF: `https://commons.wikimedia.org/wiki/File:Rotating_earth_(Very_small).gif`

The downloaded files are copied under intentionally varied names, including
international file names, before headless runs are executed.

## Model Fallback From App Settings

The explicit `--model` argument and `AI_FILE_SORTER_LIVE_LLM_MODEL` environment
variable take precedence. When neither is provided, the runner reads the normal
AI File Sorter settings file:

- `AI_FILE_SORTER_CONFIG_DIR\AIFileSorter\config.ini` when
  `AI_FILE_SORTER_CONFIG_DIR` is set.
- `%APPDATA%\AIFileSorter\config.ini` on Windows.
- `~/Library/Application Support/AIFileSorter/config.ini` on macOS.
- `~/.config/AIFileSorter/config.ini` on Linux.

For `LLMChoice=Custom`, it uses the active custom LLM's `Path`. For built-in
local choices, it looks in the configured/default model storage directory for
the downloaded model filename from the bundled `.env` download URL. Remote model
choices are not used by this suite and cause a skip unless a local GGUF is passed
explicitly.

## Additional Live Tests Worth Adding Later

- Invalid model path and corrupt model path failure reporting.
- Runtime cancellation/lock contention while a live model is loaded.
- Consistency-hint reuse across repeated categorization runs.
- Cross-run undo-plan apply/revert verification for auto-applied headless jobs.
- Large whitelist prompt pressure with hundreds of category/subcategory labels.
