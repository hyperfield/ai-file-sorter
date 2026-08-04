# 2025-10-23: chore(llm-selection-dialog): remove OpenCL mentions

## Covered commits
- `4287906` `2025-10-23` `chore(llm-selection-dialog): remove OpenCL mentions`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `M` `app/include/Utils.hpp`
- `M` `app/lib/LLMSelectionDialog.cpp`
- `M` `app/lib/LocalLLMClient.cpp`
- `M` `app/lib/Utils.cpp`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `README.md`, `app/include/Utils.hpp`, `app/lib/LLMSelectionDialog.cpp`, `app/lib/LocalLLMClient.cpp`, `app/lib/Utils.cpp`. It changed the repository support state, metadata, or supporting files in the way described by `chore(llm-selection-dialog): remove OpenCL mentions`.

Before this commit, the repository reflected the state immediately preceding `4287906`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -67,7 +67,7 @@ AI File Sorter is a powerful, cross-platform desktop application that automates
 
 - Bug fixes.
 - Minor improvements for stability.
-- OpenCL is now not supported by default in the build script for llama.cpp.
+- Removed the deprecated GPU backend from the runtime build.
 
 ### [0.9.0] - 2025-07-18
 
diff --git a/app/include/Utils.hpp b/app/include/Utils.hpp
index b800ebc..9bc1a43 100644
--- a/app/include/Utils.hpp
+++ b/app/include/Utils.hpp
@@ -39,7 +39,6 @@ public:
     static std::string get_file_name_from_url(std::string url);
     static std::string make_default_path_to_file_from_download_url(std::string url);
     static bool is_cuda_available();
-    static bool is_opencl_available(std::vector<std::string>* device_names = nullptr);
     static int get_installed_cuda_runtime_version();
     static std::string get_cudart_dll_name();
     static std::string abbreviate_user_path(const std::string& path);
```

The excerpt is taken from the commit diff for `chore(llm-selection-dialog): remove OpenCL mentions`. The most relevant surfaces are `README.md`, `app/include/Utils.hpp`, `app/lib/LLMSelectionDialog.cpp`, `app/lib/LocalLLMClient.cpp`, `app/lib/Utils.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
