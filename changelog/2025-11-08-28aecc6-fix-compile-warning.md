# 2025-11-08: fix(compile): warning

## Covered commits
- `28aecc6` `2025-11-08` `fix(compile): warning`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/LocalLLMClient.cpp`

## What changed from what, why, and how
The commit corrected behavior in `app/lib/LocalLLMClient.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(compile): warning`.

Before this commit, the repository reflected the state immediately preceding `28aecc6`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/LocalLLMClient.cpp b/app/lib/LocalLLMClient.cpp
--- a/app/lib/LocalLLMClient.cpp
+++ b/app/lib/LocalLLMClient.cpp
@@ -255,7 +255,7 @@ std::optional<BackendMemoryInfo> query_backend_memory_metrics_impl(std::string_v
     return std::nullopt;
 }
 
-std::optional<BackendMemoryInfo> resolve_backend_memory(std::string_view backend_name) {
+[[maybe_unused]] std::optional<BackendMemoryInfo> resolve_backend_memory(std::string_view backend_name) {
     if (auto& probe = backend_memory_probe_slot()) {
         return probe(backend_name);
     }
@@ -545,7 +545,7 @@ enum class PreferredBackend {
     Vulkan
 };
 
-PreferredBackend detect_preferred_backend() {
+[[maybe_unused]] PreferredBackend detect_preferred_backend() {
     const char* env = std::getenv("AI_FILE_SORTER_GPU_BACKEND");
     if (!env || *env == '\0') {
         return PreferredBackend::Auto;
```

The excerpt is taken from the commit diff for `fix(compile): warning`. The most relevant surfaces are `app/lib/LocalLLMClient.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
