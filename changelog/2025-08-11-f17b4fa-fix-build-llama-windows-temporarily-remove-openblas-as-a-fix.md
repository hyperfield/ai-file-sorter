# 2025-08-11: fix(build-llama-windows: temporarily remove openblas as a fix

## Covered commits
- `f17b4fa` `2025-08-11` `fix(build-llama-windows: temporarily remove openblas as a fix`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/scripts/build_llama_windows.ps1`

## What changed from what, why, and how
The commit corrected behavior in `app/scripts/build_llama_windows.ps1`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(build-llama-windows: temporarily remove openblas as a fix`.

Before this commit, the repository reflected the state immediately preceding `f17b4fa`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/scripts/build_llama_windows.ps1 b/app/scripts/build_llama_windows.ps1
--- a/app/scripts/build_llama_windows.ps1
+++ b/app/scripts/build_llama_windows.ps1
@@ -36,7 +36,7 @@ $cmakeArgs = @(
     "-DCMAKE_CXX_COMPILER=`"C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe`"",
     "-DBUILD_SHARED_LIBS=ON",
     "-DGGML_CUDA=$useCuda",
-    "-DGGML_BLAS=ON",
+    "-DGGML_BLAS=OFF",
     "-DGGML_BLAS_VENDOR=OpenBLAS",
     "-DGGML_OPENCL=OFF",
     "-DGGML_VULKAN=OFF",
```

The excerpt is taken from the commit diff for `fix(build-llama-windows: temporarily remove openblas as a fix`. The most relevant surfaces are `app/scripts/build_llama_windows.ps1`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
