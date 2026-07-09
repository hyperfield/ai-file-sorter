# 2025-10-29: chore(compile): add guards for compiling on non-macOS systems

## Covered commits
- `813c143` `2025-10-29` `chore(compile): add guards for compiling on non-macOS systems`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/external/llama.cpp`
- `M` `app/lib/LocalLLMClient.cpp`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/include/external/llama.cpp`, `app/lib/LocalLLMClient.cpp`. It changed the repository support state, metadata, or supporting files in the way described by `chore(compile): add guards for compiling on non-macOS systems`.

Before this commit, the repository reflected the state immediately preceding `813c143`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/external/llama.cpp b/app/include/external/llama.cpp
--- a/app/include/external/llama.cpp
+++ b/app/include/external/llama.cpp
@@ -1 +1 @@
-Subproject commit 3cfa9c3f125763305b4226bc032f1954f08990dc
+Subproject commit ee09828cb057460b369576410601a3a09279e23c
diff --git a/app/lib/LocalLLMClient.cpp b/app/lib/LocalLLMClient.cpp
index 3bb5b12..8e47f0f 100644
--- a/app/lib/LocalLLMClient.cpp
+++ b/app/lib/LocalLLMClient.cpp
@@ -98,6 +98,7 @@ struct MetalDeviceInfo {
     }
 };
 
+#if defined(GGML_USE_METAL)
 MetalDeviceInfo query_primary_gpu_device() {
     MetalDeviceInfo info;
```

The excerpt is taken from the commit diff for `chore(compile): add guards for compiling on non-macOS systems`. The most relevant surfaces are `app/include/external/llama.cpp`, `app/lib/LocalLLMClient.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
