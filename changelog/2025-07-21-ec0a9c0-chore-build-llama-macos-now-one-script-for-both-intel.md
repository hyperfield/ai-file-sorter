# 2025-07-21: chore(build_llama_macos): now one script for both Intel and ARM Macs

## Covered commits
- `ec0a9c0` `2025-07-21` `chore(build_llama_macos): now one script for both Intel and ARM Macs`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `R067` `app/scripts/build_llama_macos_arm.sh	app/scripts/build_llama_macos.sh`
- `D` `app/scripts/build_llama_macos_intel.sh`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/scripts/build_llama_macos_arm.sh	app/scripts/build_llama_macos.sh`, `app/scripts/build_llama_macos_intel.sh`. It changed the repository support state, metadata, or supporting files in the way described by `chore(build_llama_macos): now one script for both Intel and ARM Macs`.

Before this commit, the repository reflected the state immediately preceding `ec0a9c0`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/scripts/build_llama_macos_arm.sh b/app/scripts/build_llama_macos.sh
--- a/app/scripts/build_llama_macos_arm.sh
+++ b/app/scripts/build_llama_macos.sh
@@ -14,12 +14,15 @@ fi
 PRECOMPILED_LIBS_DIR="$SCRIPT_DIR/../lib/precompiled"
 HEADERS_DIR="$SCRIPT_DIR/../include/llama"
 
+ARCH=$(uname -m)
+echo "Building on architecture: $ARCH"
+
 # Enter llama.cpp directory and build
 cd "$LLAMA_DIR"
 rm -rf build
 mkdir -p build
 cmake -S . -B build \
-  -DBUILD_SHARED_LIBS=OFF \
+  -DBUILD_SHARED_LIBS=ON \
   -DGGML_METAL=ON \
   -DGGML_BLAS=ON -DGGML_BLAS_VENDOR=Accelerate \
   -DGGML_CUDA=OFF \
@@ -27,19 +30,20 @@ cmake -S . -B build \
   -DGGML_VULKAN=OFF \
   -DGGML_SYCL=OFF \
   -DGGML_HIP=OFF \
-  -DGGML_KLEIDIAI=OFF
+  -DGGML_KLEIDIAI=OFF \
+  -DBLAS_LIBRARIES="-framework Accelerate"
 
 cmake --build build --config Release -- -j$(sysctl -n hw.logicalcpu)
 
-# Copy static libs
+# Copy the resulting dynamic (.dylib) libraries
+rm -rf "$PRECOMPILED_LIBS_DIR"
 mkdir -p "$PRECOMPILED_LIBS_DIR"
-cp build/src/libllama.a "$PRECOMPILED_LIBS_DIR"
-cp build/common/libcommon.a "$PRECOMPILED_LIBS_DIR"
-cp build/ggml/src/libggml*.a "$PRECOMPILED_LIBS_DIR"
-# cp build/ggml/src/ggml-cuda/libggml*.a "$PRECOMPILED_LIBS_DIR"
-cp build/ggml/src/ggml-blas/libggml*.a "$PRECOMPILED_LIBS_DIR"
+cp build/bin/libllama.dylib "$PRECOMPILED_LIBS_DIR"
+cp build/bin/libggml*.dylib "$PRECOMPILED_LIBS_DIR"
+cp build/bin/libmtmd.dylib "$PRECOMPILED_LIBS_DIR"
 
 # Copy headers
+rm -rf "$HEADERS_DIR"
 mkdir -p "$HEADERS_DIR"
 cp include/llama.h "$HEADERS_DIR"
 cp ggml/src/*.h "$HEADERS_DIR"
```

The excerpt is taken from the commit diff for `chore(build_llama_macos): now one script for both Intel and ARM Macs`. The most relevant surfaces are `app/scripts/build_llama_macos_arm.sh	app/scripts/build_llama_macos.sh`, `app/scripts/build_llama_macos_intel.sh`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
