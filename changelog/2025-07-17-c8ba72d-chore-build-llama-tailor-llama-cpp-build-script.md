# 2025-07-17: chore(build-llama): tailor llama.cpp build script

## Covered commits
- `c8ba72d` `2025-07-17` `chore(build-llama): tailor llama.cpp build script`

## Motivation
This dependency-management commit kept bundled third-party code in sync with the capabilities or fixes the project needed. Those updates are usually required to unblock platform fixes, tests, or packaging changes in adjacent commits.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/scripts/build_llama_linux.sh`
- `M` `app/scripts/build_llama_macos_arm.sh`
- `M` `app/scripts/build_llama_macos_intel.sh`

## What changed from what, why, and how
The commit updated dependency pointers or related build references in `app/scripts/build_llama_linux.sh`, `app/scripts/build_llama_macos_arm.sh`, `app/scripts/build_llama_macos_intel.sh`. It moved the repository from older third-party revisions to newer ones needed by the surrounding feature or fix work.

Before this commit, the repository reflected the state immediately preceding `c8ba72d`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/scripts/build_llama_linux.sh b/app/scripts/build_llama_linux.sh
--- a/app/scripts/build_llama_linux.sh
+++ b/app/scripts/build_llama_linux.sh
@@ -1,7 +1,7 @@
 #!/bin/bash
 set -e
 
-# Resolve script directory (cross-shell portable)
+# Resolve script directory
 SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
 LLAMA_DIR="$SCRIPT_DIR/../include/external/llama.cpp"
 
@@ -14,24 +14,19 @@ fi
 PRECOMPILED_LIBS_DIR="$SCRIPT_DIR/../lib/precompiled"
 HEADERS_DIR="$SCRIPT_DIR/../include/llama"
 
+# Determine CUDA setting from first argument (default OFF)
+CUDASWITCH="OFF"
+if [[ "${1,,}" == "cuda=on" ]]; then
+    CUDASWITCH="ON"
+fi
+
 # Enter llama.cpp directory and build
 cd "$LLAMA_DIR"
 rm -rf build
 mkdir -p build
 
-# To compile static libs:
-# cmake -S . -B build \
-#   -DGGML_CUDA=ON \
-#   -DBUILD_SHARED_LIBS=OFF \
-#   -DCMAKE_CUDA_STANDARD=17 \
-#   -DGGML_BLAS=ON -DGGML_BLAS_VENDOR=OpenBLAS \
-#   -DGGML_OPENCL=ON -DGGML_VULKAN=OFF \
-#   -DGGML_SYCL=OFF \
-#   -DGGML_HIP=OFF \
-#   -DGGML_KLEIDIAI=OFF
-
 # To compile shared libs:
-cmake -S . -B build -DGGML_CUDA=ON \
+cmake -S . -B build -DGGML_CUDA=$CUDASWITCH \
   -DGGML_OPENCL=ON \
   -DGGML_BLAS=ON \
   -DGGML_BLAS_VENDOR=OpenBLAS \
```

The excerpt is taken from the commit diff for `chore(build-llama): tailor llama.cpp build script`. The most relevant surfaces are `app/scripts/build_llama_linux.sh`, `app/scripts/build_llama_macos_arm.sh`, `app/scripts/build_llama_macos_intel.sh`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
