# 2025-06-15: feat: Add second local LLM option in selection dialog, complete local LLM logic

## Covered commits
- `aa1f134` `2025-06-15` `feat: Add second local LLM option in selection dialog, complete local LLM logic`

## Motivation
This feature commit introduced a new user-facing or architecture-facing capability that the earlier codebase did not yet provide. The motivation was to expand the supported workflow rather than merely tune the existing one.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `.gitignore`
- `M` `app/Makefile`
- `A` `app/include/ILLMClient.hpp`
- `M` `app/include/LLMClient.hpp`
- `M` `app/include/LLMDownloader.hpp`
- `M` `app/include/LLMSelectionDialog.hpp`
- `A` `app/include/LocalLLMClient.hpp`
- `M` `app/include/MainApp.hpp`
- `M` `app/include/Types.hpp`
- `M` `app/include/Utils.hpp`
- `A` `app/include/llama/ggml-alloc.h`
- `A` `app/include/llama/ggml-backend-impl.h`
- `A` `app/include/llama/ggml-backend.h`
- `A` `app/include/llama/ggml-blas.h`
- `A` `app/include/llama/ggml-cann.h`
- `A` `app/include/llama/ggml-common.h`
- `A` `app/include/llama/ggml-cpp.h`
- `A` `app/include/llama/ggml-cpu.h`
- `A` `app/include/llama/ggml-cuda.h`
- `A` `app/include/llama/ggml-impl.h`
- `A` `app/include/llama/ggml-kompute.h`
- `A` `app/include/llama/ggml-metal.h`
- `A` `app/include/llama/ggml-opencl.h`
- `A` `app/include/llama/ggml-opt.h`
- `A` `app/include/llama/ggml-quants.h`
- `A` `app/include/llama/ggml-rpc.h`
- `A` `app/include/llama/ggml-sycl.h`
- `A` `app/include/llama/ggml-threading.h`
- `A` `app/include/llama/ggml-vulkan.h`
- `A` `app/include/llama/ggml.h`
- `A` `app/include/llama/gguf.h`
- `A` `app/include/llama/llama.h`
- `M` `app/lib/CategorizationProgressDialog.cpp`
- `M` `app/lib/LLMClient.cpp`
- `M` `app/lib/LLMDownloader.cpp`
- `M` `app/lib/LLMSelectionDialog.cpp`
- `A` `app/lib/LocalLLMClient.cpp`
- `M` `app/lib/MainApp.cpp`
- `M` `app/lib/Settings.cpp`
- `M` `app/lib/Utils.cpp`
- `M` `app/resources/.env`
- `M` `app/resources/resources.c`
- `M` `app/resources/resources.gresource`
- `A` `app/scripts/build_llama.sh`

## What changed from what, why, and how
The commit added or exposed new functionality in `.gitignore`, `app/Makefile`, `app/include/ILLMClient.hpp`, `app/include/LLMClient.hpp`, `app/include/LLMDownloader.hpp`, `app/include/LLMSelectionDialog.hpp`, `app/include/LocalLLMClient.hpp`, `app/include/MainApp.hpp`, and 36 more file(s). It changed the project from not having the capability described by `feat: Add second local LLM option in selection dialog, complete local LLM logic` to having a concrete implementation of it.

Before this commit, the repository reflected the state immediately preceding `aa1f134`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/.gitignore b/.gitignore
--- a/.gitignore
+++ b/.gitignore
@@ -40,6 +40,9 @@ encryption.ini
 *.a
 *.lib
 
+# Third-part libraries
+app/include/external/llama.cpp/
+
 # Executables
 *.exe
 *.out
diff --git a/app/Makefile b/app/Makefile
index 38aa6d8..7f8bf29 100644
--- a/app/Makefile
+++ b/app/Makefile
@@ -12,7 +12,10 @@ ifeq ($(UNAME), Linux)
     TARGET := $(BIN_DIR)/aifilesorter
     INSTALL_DIR := /usr/local/bin
     RESOURCE_DIR := /usr/local/share/aifilesorter
-    LDFLAGS += -lcurl -ljsoncpp -lsqlite3 -lcrypto -lfmt -lspdlog -lssl
+    LDFLAGS += -lcurl -ljsoncpp -lsqlite3 -lcrypto -lfmt -lspdlog -lssl -lllama -lcommon \
+		-lggml -lggml-cpu -lggml-cuda -lggml-base -lggml-blas \
+  		-lopenblas -lcudart -lcuda -lcublas \
+  		-pthread -fopenmp
 else ifeq ($(UNAME), Darwin)
     PLATFORM := MacOS
     CXXFLAGS += -DMACOS
```

The excerpt is taken from the commit diff for `feat: Add second local LLM option in selection dialog, complete local LLM logic`. The most relevant surfaces are `.gitignore`, `app/Makefile`, `app/include/ILLMClient.hpp`, `app/include/LLMClient.hpp`, `app/include/LLMDownloader.hpp`, and 39 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
