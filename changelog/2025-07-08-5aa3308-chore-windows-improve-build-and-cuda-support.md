# 2025-07-08: chore(windows): improve build and CUDA support

## Covered commits
- `5aa3308` `2025-07-08` `chore(windows): improve build and CUDA support`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
- Updated Makefile to support compilation on Windows with MinGW and MSVC
- Added PowerShell script to build llama.cpp on Windows with CMake and CUDA
- Fixed CUDA runtime detection logic for cudart DLL naming and loading
- Updated README with Windows-specific instructions and notes

## Files changed
- `M` `.gitignore`
- `M` `README.md`
- `M` `app/Makefile`
- `M` `app/include/external/llama.cpp`
- `D` `app/include/llama/ggml-alloc.h`
- `D` `app/include/llama/ggml-backend-impl.h`
- `D` `app/include/llama/ggml-backend.h`
- `D` `app/include/llama/ggml-blas.h`
- `D` `app/include/llama/ggml-cann.h`
- `D` `app/include/llama/ggml-common.h`
- `D` `app/include/llama/ggml-cpp.h`
- `D` `app/include/llama/ggml-cpu.h`
- `D` `app/include/llama/ggml-cuda.h`
- `D` `app/include/llama/ggml-impl.h`
- `D` `app/include/llama/ggml-kompute.h`
- `D` `app/include/llama/ggml-metal.h`
- `D` `app/include/llama/ggml-opencl.h`
- `D` `app/include/llama/ggml-opt.h`
- `D` `app/include/llama/ggml-quants.h`
- `D` `app/include/llama/ggml-rpc.h`
- `D` `app/include/llama/ggml-sycl.h`
- `D` `app/include/llama/ggml-threading.h`
- `D` `app/include/llama/ggml-vulkan.h`
- `D` `app/include/llama/ggml.h`
- `D` `app/include/llama/gguf.h`
- `D` `app/include/llama/llama.h`
- `M` `app/lib/Utils.cpp`
- `A` `app/scripts/build_llama_windows.ps1`
- `D` `app/scripts/build_llama_windows.sh`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `.gitignore`, `README.md`, `app/Makefile`, `app/include/external/llama.cpp`, `app/include/llama/ggml-alloc.h`, `app/include/llama/ggml-backend-impl.h`, `app/include/llama/ggml-backend.h`, `app/include/llama/ggml-blas.h`, and 21 more file(s). It changed the repository support state, metadata, or supporting files in the way described by `chore(windows): improve build and CUDA support`.

Before this commit, the repository reflected the state immediately preceding `5aa3308`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/.gitignore b/.gitignore
--- a/.gitignore
+++ b/.gitignore
@@ -84,4 +84,7 @@ todos_md/
 # Mac files
 .DS_Store
 
+# Ignore llama headers (generated or from external)
+app/include/llama/*.h
+
 # End of https://www.toptal.com/developers/gitignore/api/c++,visualstudiocode
diff --git a/README.md b/README.md
index 9fc34d8..9aecfae 100644
--- a/README.md
+++ b/README.md
@@ -77,6 +77,8 @@ You can also now launch `Git Bash` from Start Menu.
 ##### Clone the repository
 
     git clone https://github.com/hyperfield/ai-file-sorter.git
+    cd ai-file-sorter
+    git submodule update --init --recursive --remote
 
 ##### Navigate into the directory
```

The excerpt is taken from the commit diff for `chore(windows): improve build and CUDA support`. The most relevant surfaces are `.gitignore`, `README.md`, `app/Makefile`, `app/include/external/llama.cpp`, `app/include/llama/ggml-alloc.h`, and 24 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
