# 2026-02-01: docs(readme): update build and dependency notes

## Covered commits
- `1f936ab` `2026-02-01` `docs(readme): update build and dependency notes`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `1f936ab`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -178,7 +178,7 @@ Document analysis uses the same selected LLM (local or remote) to extract text f
 - Office/OpenOffice: `.docx`, `.xlsx`, `.pptx`, `.odt`, `.ods`, `.odp` (embedded libzip+pugixml in bundled builds; CLI fallback uses `unzip` if you build without vendored libs)
 - Legacy binary formats like `.doc`, `.xls`, `.ppt` are not currently supported.
 
-Source builds: run `app/scripts/vendor_doc_deps.sh` (or `app\\scripts\\vendor_doc_deps.ps1` on Windows) to populate `external/` and enable the embedded extractors.
+Source builds: embedded extractors are used when `external/` contains the vendored libs; otherwise the app falls back to CLI tools (`pdftotext`, `unzip`) for document extraction.
 
 ### Main window options (documents)
 
@@ -196,7 +196,7 @@ Source builds: run `app/scripts/vendor_doc_deps.sh` (or `app\\scripts\\vendor_do
 - **Compiler**: A C++20-capable compiler (`g++` or `clang++`).
 - **Qt 6**: Core, Gui, Widgets modules and the Qt resource compiler (`qt6-base-dev` / `qt6-tools` on Linux, `brew install qt` on macOS).
 - **Libraries**: `curl`, `sqlite3`, `fmt`, `spdlog`, and the prebuilt `llama` libraries shipped under `app/lib/precompiled`.
-- **Document analysis libraries** (vendored): PDFium, libzip, and pugixml. For source builds, run `app/scripts/vendor_doc_deps.sh` (or `app\\scripts\\vendor_doc_deps.ps1` on Windows) to populate `external/` and enable embedded extraction.
+- **Document analysis libraries** (vendored): PDFium, libzip, and pugixml. Source builds use the embedded extractors when `external/` is populated; otherwise they fall back to `pdftotext`/`unzip`.
 - **Optional GPU backends**: A Vulkan 1.2+ runtime (preferred) or CUDA 12.x for NVIDIA cards. `StartAiFileSorter.exe`/`run_aifilesorter.sh` auto-detect the best available backend and fall back to CPU/OpenBLAS automatically, so CUDA is never required to run the app.
 - **Git** (optional): For cloning this repository. Archives can also be downloaded.
 - **OpenAI or Gemini API key** (optional): Required only when using the remote ChatGPT or Gemini workflow.
```

The excerpt is taken from the commit diff for `docs(readme): update build and dependency notes`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
