# 2025-11-12: chore(run-aifilesorter-linux): Vulkan takes preference over CUDA

## Covered commits
- `3f8309c` `2025-11-12` `chore(run-aifilesorter-linux): Vulkan takes preference over CUDA`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/external/llama.cpp`
- `M` `app/scripts/run_aifilesorter.sh.in`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/include/external/llama.cpp`, `app/scripts/run_aifilesorter.sh.in`. It changed the repository support state, metadata, or supporting files in the way described by `chore(run-aifilesorter-linux): Vulkan takes preference over CUDA`.

Before this commit, the repository reflected the state immediately preceding `3f8309c`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/external/llama.cpp b/app/include/external/llama.cpp
--- a/app/include/external/llama.cpp
+++ b/app/include/external/llama.cpp
@@ -1 +1 @@
-Subproject commit 7675c555a13c9f473249e59a54db35032ce8e0fc
+Subproject commit ee09828cb057460b369576410601a3a09279e23c
diff --git a/app/scripts/run_aifilesorter.sh.in b/app/scripts/run_aifilesorter.sh.in
index d62718c..f860462 100644
--- a/app/scripts/run_aifilesorter.sh.in
+++ b/app/scripts/run_aifilesorter.sh.in
@@ -63,10 +63,10 @@ USE_VULKAN=0
 GPU_BACKEND="cpu"
 GGML_VARIANT="wocuda"
 LLAMA_DEVICE=""
-if [ -n "$SELECTED_CUDA_DIR" ]; then
-    USE_CUDA=1
-elif [ -n "$SELECTED_VK_DIR" ]; then
+if [ -n "$SELECTED_VK_DIR" ]; then
     USE_VULKAN=1
+elif [ -n "$SELECTED_CUDA_DIR" ]; then
+    USE_CUDA=1
 fi
 
 if [ "$CUDA_OVERRIDE" = "on" ]; then
```

The excerpt is taken from the commit diff for `chore(run-aifilesorter-linux): Vulkan takes preference over CUDA`. The most relevant surfaces are `app/include/external/llama.cpp`, `app/scripts/run_aifilesorter.sh.in`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
