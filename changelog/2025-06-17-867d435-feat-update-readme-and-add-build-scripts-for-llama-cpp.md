# 2025-06-17: feat: update readme and add build scripts for llama.cpp on Windows and macOS

## Covered commits
- `867d435` `2025-06-17` `feat: update readme and add build scripts for llama.cpp on Windows and macOS`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `R085` `app/scripts/build_llama.sh	app/scripts/build_llama_linux.sh`
- `A` `app/scripts/build_llama_macos_arm.sh`
- `A` `app/scripts/build_llama_macos_intel.sh`
- `A` `app/scripts/build_llama_windows.sh`

## What changed from what, why, and how
The commit updated documentation artifacts touching `app/scripts/build_llama.sh	app/scripts/build_llama_linux.sh`, `app/scripts/build_llama_macos_arm.sh`, `app/scripts/build_llama_macos_intel.sh`, `app/scripts/build_llama_windows.sh`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `867d435`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/scripts/build_llama.sh b/app/scripts/build_llama_linux.sh
--- a/app/scripts/build_llama.sh
+++ b/app/scripts/build_llama_linux.sh
@@ -22,7 +22,11 @@ cmake -S . -B build \
   -DGGML_CUDA=ON \
   -DBUILD_SHARED_LIBS=OFF \
   -DCMAKE_CUDA_STANDARD=17 \
-  -DGGML_BLAS=OFF -DGGML_BLAS_VENDOR=OpenBLAS
+  -DGGML_BLAS=ON -DGGML_BLAS_VENDOR=OpenBLAS \
+  -DGGML_OPENCL=ON -DGGML_VULKAN=OFF \
+  -DGGML_SYCL=OFF \
+  -DGGML_HIP=OFF \
+  -DGGML_KLEIDIAI=OFF
 
 cmake --build build --config Release -- -j$(nproc)
 
diff --git a/app/scripts/build_llama_macos_arm.sh b/app/scripts/build_llama_macos_arm.sh
new file mode 100644
index 0000000..1272152
--- /dev/null
+++ b/app/scripts/build_llama_macos_arm.sh
@@ -0,0 +1,46 @@
+#!/bin/bash
+set -e
+
+# Resolve script directory (cross-shell portable)
+SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
+LLAMA_DIR="$SCRIPT_DIR/../include/external/llama.cpp"
+
+if [ ! -d "$LLAMA_DIR" ]; then
+    echo "Missing llama.cpp submodule. Please run:"
+    echo "  git submodule update --init --recursive"
+    exit 1
+fi
+
+PRECOMPILED_LIBS_DIR="$SCRIPT_DIR/../lib/precompiled"
+HEADERS_DIR="$SCRIPT_DIR/../include/llama"
+
+# Enter llama.cpp directory and build
+cd "$LLAMA_DIR"
+rm -rf build
+mkdir -p build
+cmake -S . -B build \
+  -DBUILD_SHARED_LIBS=OFF \
+  -DGGML_METAL=ON \
+  -DGGML_BLAS=ON -DGGML_BLAS_VENDOR=Accelerate \
+  -DGGML_CUDA=OFF \
+  -DGGML_OPENCL=OFF \
+  -DGGML_VULKAN=OFF \
+  -DGGML_SYCL=OFF \
+  -DGGML_HIP=OFF \
+  -DGGML_KLEIDIAI=OFF
+
+cmake --build build --config Release -- -j$(sysctl -n hw.logicalcpu)
+
+# Copy static libs
+mkdir -p "$PRECOMPILED_LIBS_DIR"
+cp build/src/libllama.a "$PRECOMPILED_LIBS_DIR"
+cp build/common/libcommon.a "$PRECOMPILED_LIBS_DIR"
+cp build/ggml/src/libggml*.a "$PRECOMPILED_LIBS_DIR"
+# cp build/ggml/src/ggml-cuda/libggml*.a "$PRECOMPILED_LIBS_DIR"
+cp build/ggml/src/ggml-blas/libggml*.a "$PRECOMPILED_LIBS_DIR"
+
+# Copy headers
+mkdir -p "$HEADERS_DIR"
+cp include/llama.h "$HEADERS_DIR"
+cp ggml/src/*.h "$HEADERS_DIR"
+cp ggml/include/*.h "$HEADERS_DIR"
\ No newline at end of file
```

The excerpt is taken from the commit diff for `feat: update readme and add build scripts for llama.cpp on Windows and macOS`. The most relevant surfaces are `app/scripts/build_llama.sh	app/scripts/build_llama_linux.sh`, `app/scripts/build_llama_macos_arm.sh`, `app/scripts/build_llama_macos_intel.sh`, `app/scripts/build_llama_windows.sh`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
