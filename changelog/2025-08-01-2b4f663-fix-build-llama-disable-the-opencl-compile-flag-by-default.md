# 2025-08-01: fix(build-llama): disable the OpenCL compile flag by default

## Covered commits
- `2b4f663` `2025-08-01` `fix(build-llama): disable the OpenCL compile flag by default`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/scripts/build_llama_linux.sh`
- `M` `app/scripts/build_llama_windows.ps1`

## What changed from what, why, and how
The commit corrected behavior in `app/scripts/build_llama_linux.sh`, `app/scripts/build_llama_windows.ps1`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(build-llama): disable the OpenCL compile flag by default`.

Before this commit, the repository reflected the state immediately preceding `2b4f663`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/scripts/build_llama_linux.sh b/app/scripts/build_llama_linux.sh
--- a/app/scripts/build_llama_linux.sh
+++ b/app/scripts/build_llama_linux.sh
@@ -29,7 +29,7 @@ mkdir -p build
 echo "Inside script: CC=$CC, CXX=$CXX"
 
 cmake -S . -B build -DGGML_CUDA=$CUDASWITCH \
-      -DGGML_OPENCL=ON \
+      -DGGML_OPENCL=OFF \
       -DGGML_BLAS=ON \
       -DGGML_BLAS_VENDOR=OpenBLAS \
       -DBUILD_SHARED_LIBS=ON \
diff --git a/app/scripts/build_llama_windows.ps1 b/app/scripts/build_llama_windows.ps1
index 8e60eff..ad6db7d 100644
--- a/app/scripts/build_llama_windows.ps1
+++ b/app/scripts/build_llama_windows.ps1
@@ -47,7 +47,7 @@ $libDir = "$cudaRoot/lib/x64/cudart.lib"
     -DGGML_BLAS_VENDOR=OpenBLAS `
     -DBLAS_LIBRARIES="C:/msys64/mingw64/lib/libopenblas.a" `
     -DBLAS_INCLUDE_DIR="C:/msys64/mingw64/include/openblas/" `
-    -DGGML_OPENCL=ON `
+    -DGGML_OPENCL=OFF `
     -DGGML_VULKAN=OFF `
     -DGGML_SYCL=OFF `
     -DGGML_HIP=OFF `
```

The excerpt is taken from the commit diff for `fix(build-llama): disable the OpenCL compile flag by default`. The most relevant surfaces are `app/scripts/build_llama_linux.sh`, `app/scripts/build_llama_windows.ps1`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
