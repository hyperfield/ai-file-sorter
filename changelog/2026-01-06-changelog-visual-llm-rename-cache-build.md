# 2026-01-06 - Visual LLM, rename cache, GPU/backends, build/tests

This changelog captures the changes I implemented during this chat session. The goals were to:

- keep partial work when Cancel is pressed (finish the current item, then stop cleanly),
- show visual LLM batch progress during image decoding,
- support image rename-only flows (and reuse the image analysis output for categorization),
- persist suggested filenames in the cache database,
- fix rename-only behavior after Undo and surface accurate statuses in the Review dialog,
- add explicit GPU backend selection and safer auto-estimates,
- stabilize builds for mtmd progress callbacks and mtmd-cli, and
- document runtime knobs and add tests to cover the new flows.

## Visual LLM analysis pipeline (image handling, progress, cancel)

Motivation: image analysis needs visible progress, should allow Cancel to stop after the current file, and should reuse image analysis output rather than recomputing prompts. It also needs a clean rename-only path.

### New analyzer for image description + filename

Created `app/include/LlavaImageAnalyzer.hpp` and `app/lib/LlavaImageAnalyzer.cpp` to encapsulate LLaVA usage and filename suggestions. It generates a description, then a short filename prompt, and normalizes the name to the original extension:

```cpp
LlavaImageAnalysisResult LlavaImageAnalyzer::analyze(const std::filesystem::path& image_path) {
    const std::string description = infer_text(bitmap.get(),
                                               build_description_prompt(),
                                               settings_.n_predict);

    const std::string raw_filename = infer_text(nullptr,
                                                build_filename_prompt(description),
                                                settings_.n_predict);
    std::string filename_base = sanitize_filename(raw_filename, kMaxFilenameWords, kMaxFilenameLength);
    if (filename_base.empty()) {
        filename_base = sanitize_filename(description, kMaxFilenameWords, kMaxFilenameLength);
    }

    LlavaImageAnalysisResult result;
    result.description = description;
    result.suggested_name = normalize_filename(filename_base, image_path);
    return result;
}
```

### Batch progress callback for image decoding

The visual decoder is broken into batches, so I hooked mtmd progress callbacks to report `current_batch/total_batches` back to the UI:

```cpp
void LlavaImageAnalyzer::mtmd_progress_callback(const char* name,
                                                int32_t current_batch,
                                                int32_t total_batches,
                                                void* user_data) {
    if (name && std::strcmp(name, "image") != 0) {
        return;
    }
    auto* self = static_cast<LlavaImageAnalyzer*>(user_data);
    if (self->settings_.batch_progress) {
        self->settings_.batch_progress(current_batch, total_batches);
    }
}
```

The callback is registered only when the progress API is available (see build fixes below):

```cpp
#ifdef AI_FILE_SORTER_MTMD_PROGRESS_CALLBACK
struct ProgressGuard {
    bool active{false};
    ProgressGuard(bool enabled, LlavaImageAnalyzer* self) : active(enabled) {
        if (active) {
            mtmd_helper_set_progress_callback(&LlavaImageAnalyzer::mtmd_progress_callback, self);
        }
    }
    ~ProgressGuard() {
        if (active) {
            mtmd_helper_set_progress_callback(nullptr, nullptr);
        }
    }
};
#endif
```

### Main analysis flow changes (cancel, batch progress, rename-only, prompt override)

`app/lib/MainApp.cpp` was updated to:

- track a `stop_requested` flag that is latched when Cancel is pressed, allowing the current file to finish before stopping;
- split image entries vs. non-image entries;
- run image analysis first, store per-image metadata in `image_info`, and reuse that for categorization;
- emit progress lines for image decoding batches;
- support "rename images only" without running text categorization for those images.

```cpp
bool stop_requested = false;
auto update_stop = [this, &stop_requested]() {
    if (!stop_requested && should_abort_analysis()) {
        stop_requested = true;
    }
    return stop_requested;
};

LlavaImageAnalyzer::Settings vision_settings;
vision_settings.batch_progress = [this](int current_batch, int total_batches) {
    const double percent = (static_cast<double>(current_batch) /
                            static_cast<double>(total_batches)) * 100.0;
    append_progress(fmt::format("[VISION] Decoding image batch {}/{} ({:.2f}%)",
                                current_batch, total_batches, percent));
};
```

When image analysis is enabled, the code now feeds a prompt override into the categorization step so the LLM uses the image-derived prompt (instead of re-deriving it):

```cpp
auto override_provider = [&image_info](const FileEntry& entry)
    -> std::optional<CategorizationService::PromptOverride> {
    const auto it = image_info.find(entry.file_name);
    if (it == image_info.end()) {
        return std::nullopt;
    }
    return CategorizationService::PromptOverride{it->second.prompt_name, it->second.prompt_path};
};
```

To support this override, `app/include/CategorizationService.hpp` and `app/lib/CategorizationService.cpp` gained a new `PromptOverride` type and provider:

```cpp
struct PromptOverride {
    std::string name;
    std::string path;
};
using PromptOverrideProvider = std::function<std::optional<PromptOverride>(const FileEntry&)>;
```

## Review dialog: rename-only flow and status reporting

Motivation: the Review dialog needed to distinguish rename-only results and show accurate statuses ("Renamed" vs. "Renamed & Moved"). It also needed to apply suggested filenames and allow a rename to be performed again after Undo.

### Status and columns

`app/include/CategorizationDialog.hpp` now includes separate status enums and explicit columns for suggested names and preview:

```cpp
enum class RowStatus {
    None = 0,
    Moved,
    Renamed,
    RenamedAndMoved,
    Skipped,
    NotSelected,
    Preview
};
```

`app/lib/CategorizationDialog.cpp` sets the status based on whether a rename happened and whether a move occurred:

```cpp
if (renamed && moved) {
    status = RowStatus::RenamedAndMoved;
} else if (renamed) {
    status = RowStatus::Renamed;
} else {
    status = RowStatus::Moved;
}
```

and renders it for the UI:

```cpp
case RowStatus::Renamed:
    item->setText(tr("Renamed"));
    break;
case RowStatus::RenamedAndMoved:
    item->setText(tr("Renamed & Moved"));
    break;
```

### Rename-only execution + cache updates

Rename-only rows now perform a filesystem rename, update UI status, update the cache, and push an Undo record:

```cpp
if (rename_only) {
    std::filesystem::rename(source_path, dest_path);
    update_status_column(row_index, true, true, rename_active, false);
    ...
    db_manager->remove_file_categorization(base_dir, file_name, file_type);
    db_manager->insert_or_update_file_with_categorization(
        destination_name,
        file_type_label,
        base_dir,
        resolved,
        used_consistency_hints,
        destination_name,
        true);
}
```

### Destination name resolution

If a user edits the suggested filename without extension, the original extension is preserved:

```cpp
if (!candidate_path.has_extension() && original_path.has_extension()) {
    return Utils::path_to_utf8(candidate_path) + original_path.extension().string();
}
```

### Move + rename in one step

`app/include/MovableCategorizedFile.hpp` and `app/lib/MovableCategorizedFile.cpp` now accept an explicit destination filename so rename+move happens in a single operation:

```cpp
MovableCategorizedFile(const std::string& dir_path,
                       const std::string& cat,
                       const std::string& subcat,
                       const std::string& file_name,
                       const std::string& destination_name = std::string());
```

## Cache DB updates (suggested filenames and rename-only)

Motivation: suggested filenames were lost between runs because they were not persisted. Rename-only entries should be stored even when category/subcategory are empty.

### Data model changes

`app/include/Types.hpp` now includes suggested-name and rename-only flags:

```cpp
struct CategorizedFile {
    ...
    std::string suggested_name;
    bool rename_only{false};
};
```

### Database schema and queries

`app/lib/DatabaseManager.cpp` adds the `suggested_name` column and includes it in insert/update and load paths:

```cpp
INSERT INTO file_categorization
    (file_name, file_type, dir_path, category, subcategory, suggested_name,
     taxonomy_id, categorization_style, rename_only)
VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
ON CONFLICT(file_name, file_type, dir_path)
DO UPDATE SET
    category = excluded.category,
    subcategory = excluded.subcategory,
    suggested_name = excluded.suggested_name,
    taxonomy_id = excluded.taxonomy_id,
    categorization_style = excluded.categorization_style,
    rename_only = excluded.rename_only;
```

```cpp
SELECT dir_path, file_name, file_type, category, subcategory, suggested_name,
       taxonomy_id, categorization_style, rename_only
FROM file_categorization WHERE dir_path = ?;
```

### Recording suggested names from the UI

`app/lib/CategorizationDialog.cpp` now passes the in-table suggested name into cache updates, so the suggestion persists even when the user does not rename immediately:

```cpp
const auto* suggested_item = model->item(row, ColumnSuggestedName);
const std::string suggested_name = suggested_item
                                       ? suggested_item->text().toStdString()
                                       : std::string();
...
db_manager->insert_or_update_file_with_categorization(
    file_name, file_type_label, file_path, resolved, used_consistency, suggested_name);
```

## GPU backend selection and auto-estimation

Motivation: allow explicit backend choice (cpu/vulkan/cuda) and predictable auto-selection with safety checks. Also make these options visible in the docs.

`app/lib/LocalLLMClient.cpp` now supports `AI_FILE_SORTER_GPU_BACKEND`, `AI_FILE_SORTER_N_GPU_LAYERS`, and `AI_FILE_SORTER_CTX_TOKENS`:

```cpp
const char* env = std::getenv("AI_FILE_SORTER_GPU_BACKEND");
...
if (value == "cuda") {
    return PreferredBackend::Cuda;
}
if (value == "vulkan") {
    return PreferredBackend::Vulkan;
}
if (value == "cpu") {
    return PreferredBackend::Cpu;
}
```

```cpp
if (try_parse_env_int("AI_FILE_SORTER_N_GPU_LAYERS", parsed)) {
    return parsed;
}
...
if (override_layers <= 0) {
    params.n_gpu_layers = 0;
    logger->info("Vulkan backend requested but AI_FILE_SORTER_N_GPU_LAYERS <= 0; using CPU instead.");
}
```

`README.md` now documents these options (see Docs section below).

## Build fixes for mtmd progress callback and mtmd-cli

Motivation: builds were failing when `mtmd_helper_set_progress_callback` was absent in the precompiled `libmtmd` and when mtmd-cli could not find `arg.h`. The fixes make these failure modes automatic and clearer.

### Auto-detect mtmd progress callback

`app/Makefile` now detects whether the symbol exists and defines `AI_FILE_SORTER_MTMD_PROGRESS_CALLBACK` only when available:

```make
MTMD_PROGRESS_CALLBACK ?= auto
...
if [ -e "$$lib" ] && nm -D --defined-only "$$lib" 2>/dev/null | grep -q "mtmd_helper_set_progress_callback"; then \
    echo 1; exit 0; \
fi; \
...
ifeq ($(MTMD_PROGRESS_CALLBACK),1)
CXXFLAGS += -DAI_FILE_SORTER_MTMD_PROGRESS_CALLBACK
else
CXXFLAGS += -UAI_FILE_SORTER_MTMD_PROGRESS_CALLBACK
endif
```

### Fix QRC file parsing in Makefile

The QRC resource extraction now uses a sed delimiter that does not conflict with paths:

```make
QRC_RESOURCES := $(shell sed -n -E 's|.*<file>([^<]*)</file>.*|\1|p' $(QRC_FILE))
```

### Ensure mtmd-cli builds `arg.h`

`app/CMakeLists.txt` now forces `LLAMA_BUILD_COMMON ON`, which is required to build mtmd-cli dependencies:

```cmake
set(LLAMA_BUILD_COMMON ON CACHE BOOL "llama: build common utils library" FORCE)
```

## Tests and test hooks

Motivation: verify rename-only caching and undo behavior, and enable deterministic UI tests for the visual LLM flows.

### New Catch2 test for rename-only caching

`tests/unit/test_database_manager_rename_only.cpp` ensures suggested names are cached alongside rename-only entries:

```cpp
REQUIRE(db.insert_or_update_file_with_categorization(
    "rename.png", "F", dir_path, empty, false, suggested_name, true));
...
CHECK(entries.front().suggested_name == suggested_name);
```

### Rename-only undo/redo test

`tests/unit/test_categorization_dialog.cpp` validates that a rename can be applied again after Undo:

```cpp
dialog.test_trigger_confirm();
dialog.test_trigger_undo();
dialog.test_trigger_confirm();
REQUIRE(std::filesystem::exists(destination));
```

### UI test hooks

To make these tests possible, `app/include/CategorizationDialog.hpp` gained test-only entry/trigger helpers, and `app/include/LLMSelectionDialogTestAccess.hpp` plus `app/include/MainApp.hpp` expose hooks to simulate the visual LLM selection flow without user interaction.

`app/CMakeLists.txt` now includes the new tests and ensures Catch2 is pulled in:

```cmake
add_executable(ai_file_sorter_tests
    ${APP_LIB_SOURCES}
    ...
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/test_database_manager_rename_only.cpp"
)
```

## Documentation updates

Motivation: make runtime configuration discoverable without reading code.

`README.md` now lists all the relevant environment variables:

```md
- `AI_FILE_SORTER_GPU_BACKEND` - select GPU backend: `auto` (default), `vulkan`, `cuda`, or `cpu`.
- `AI_FILE_SORTER_N_GPU_LAYERS` - override `n_gpu_layers` for llama.cpp; `-1` = auto, `0` = force CPU.
- `AI_FILE_SORTER_CTX_TOKENS` - override local LLM context length (default 2048; clamped 512-8192).
...
- `AI_FILE_SORTER_VISUAL_USE_GPU` - force visual encoder GPU usage (`1`) or CPU (`0`). Defaults to auto.
```

---

New files introduced in this session include:
- `app/include/LlavaImageAnalyzer.hpp`
- `app/lib/LlavaImageAnalyzer.cpp`
- `app/include/LlamaModelParams.hpp`
- `tests/unit/test_database_manager_rename_only.cpp`

Key files updated include:
- `app/lib/MainApp.cpp`
- `app/lib/CategorizationDialog.cpp`
- `app/lib/DatabaseManager.cpp`
- `app/lib/LocalLLMClient.cpp`
- `app/Makefile`
- `app/CMakeLists.txt`
- `README.md`
