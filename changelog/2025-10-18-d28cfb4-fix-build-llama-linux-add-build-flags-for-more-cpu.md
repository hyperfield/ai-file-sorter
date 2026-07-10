# 2025-10-18: fix(build-llama-linux): add build flags for more CPU architecture neutrality

## Covered commits
- `d28cfb4` `2025-10-18` `fix(build-llama-linux): add build flags for more CPU architecture neutrality`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/scripts/build_llama_linux.sh`

## What changed from what, why, and how
The commit corrected behavior in `app/scripts/build_llama_linux.sh`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(build-llama-linux): add build flags for more CPU architecture neutrality`.

Before this commit, the repository reflected the state immediately preceding `d28cfb4`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/scripts/build_llama_linux.sh b/app/scripts/build_llama_linux.sh
--- a/app/scripts/build_llama_linux.sh
+++ b/app/scripts/build_llama_linux.sh
@@ -33,7 +33,10 @@ cmake -S . -B build -DGGML_CUDA=$CUDASWITCH \
       -DGGML_BLAS=ON \
       -DGGML_BLAS_VENDOR=OpenBLAS \
       -DBUILD_SHARED_LIBS=ON \
-      -DCMAKE_CUDA_HOST_COMPILER=/usr/bin/g++-10
+      -DCMAKE_CUDA_HOST_COMPILER=/usr/bin/g++-10 \
+      -DGGML_NATIVE=OFF \
+      -DCMAKE_C_FLAGS="-mavx2 -mfma" \
+      -DCMAKE_CXX_FLAGS="-mavx2 -mfma"
 
 cmake --build build --config Release -- -j$(nproc)
```

The excerpt is taken from the commit diff for `fix(build-llama-linux): add build flags for more CPU architecture neutrality`. The most relevant surfaces are `app/scripts/build_llama_linux.sh`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
