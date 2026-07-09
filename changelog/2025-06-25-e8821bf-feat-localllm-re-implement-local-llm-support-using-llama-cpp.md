# 2025-06-25: feat(localllm): re-implement local LLM support using llama.cpp shared libs

## Covered commits
- `e8821bf` `2025-06-25` `feat(localllm): re-implement local LLM support using llama.cpp shared libs`

## Motivation
This dependency-management commit kept bundled third-party code in sync with the capabilities or fixes the project needed. Those updates are usually required to unblock platform fixes, tests, or packaging changes in adjacent commits.

## Commit message body
Switch to shared libraries for CUDA, OpenCL, and OpenBLAS to utilise automatic backend selection by llama.cpp.
Update Makefile and llama.cpp build scripts accordingly.

## Files changed
- `M` `app/Makefile`
- `M` `app/include/Types.hpp`
- `M` `app/include/Utils.hpp`
- `M` `app/lib/LocalLLMClient.cpp`
- `M` `app/lib/Utils.cpp`
- `M` `app/scripts/build_llama_linux.sh`
- `M` `app/scripts/build_llama_macos_intel.sh`

## What changed from what, why, and how
The commit updated dependency pointers or related build references in `app/Makefile`, `app/include/Types.hpp`, `app/include/Utils.hpp`, `app/lib/LocalLLMClient.cpp`, `app/lib/Utils.cpp`, `app/scripts/build_llama_linux.sh`, `app/scripts/build_llama_macos_intel.sh`. It moved the repository from older third-party revisions to newer ones needed by the surrounding feature or fix work.

Before this commit, the repository reflected the state immediately preceding `e8821bf`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/Makefile b/app/Makefile
--- a/app/Makefile
+++ b/app/Makefile
@@ -8,14 +8,19 @@ RESOURCES = resources/resources.c
 
 ifeq ($(UNAME), Linux)
     PLATFORM := Linux
-    CXXFLAGS += -DLINUX
+    CXXFLAGS += -DLINUX -DENABLE_CUDA -DENABLE_OPENCL
     TARGET := $(BIN_DIR)/aifilesorter
     INSTALL_DIR := /usr/local/bin
     RESOURCE_DIR := /usr/local/share/aifilesorter
-    LDFLAGS += -lcurl -ljsoncpp -lsqlite3 -lcrypto -lfmt -lspdlog -lssl -lllama -lcommon \
-		-lggml -lggml-cpu -lggml-cuda -lggml-base -lggml-blas \
-  		-lopenblas -lcudart -lcuda -lcublas \
-  		-pthread -fopenmp
+    # LDFLAGS += -lcurl -ljsoncpp -lsqlite3 -lcrypto -lfmt -lspdlog -lssl -lllama -lcommon \
+	# 	-lggml -lggml-cpu -lggml-cuda -lggml-base -lggml-blas \
+  	# 	-lopenblas -lcudart -lcuda -lcublas \
+  	# 	-pthread -fopenmp
+
+	LDFLAGS += -lcurl -ljsoncpp -lsqlite3 -lcrypto -lfmt -lspdlog -lssl -lllama -lggml -pthread
+	LDFLAGS += -Wl,-rpath,'$$ORIGIN/../lib/precompiled'
+
+
 else ifeq ($(UNAME), Darwin)
     PLATFORM := MacOS
     CXXFLAGS += -DMACOS
@@ -23,9 +28,18 @@ else ifeq ($(UNAME), Darwin)
     INSTALL_DIR := /usr/local/bin
     RESOURCE_DIR := /usr/local/share/aifilesorter
     LDFLAGS += -lcurl -ljsoncpp -lsqlite3 -lcrypto -lfmt -lspdlog -lssl
+
+    IS_APPLE_SILICON := $(shell sysctl -n machdep.cpu.brand_string | grep -i "Apple" > /dev/null && echo 1 || echo 0)
+
+    ifeq ($(IS_APPLE_SILICON), 1)
+        CXXFLAGS += -DENABLE_METAL
+    else
+        CXXFLAGS += -DENABLE_OPENCL
+    endif
+
 else ifeq ($(UNAME), MINGW32_NT)
     PLATFORM := Windows (32-bit)
-    CXXFLAGS += -DWINDOWS
+    CXXFLAGS += -DWINDOWS -DENABLE_OPENCL
     TARGET := $(BIN_DIR)/AI\ File\ Sorter.exe
     INSTALL_DIR := C:/Program\ Files\ (x86)/AiFileSorter
     RESOURCE_DIR := C:/Program\ Files\ (x86)/AiFileSorter/resources
```

The excerpt is taken from the commit diff for `feat(localllm): re-implement local LLM support using llama.cpp shared libs`. The most relevant surfaces are `app/Makefile`, `app/include/Types.hpp`, `app/include/Utils.hpp`, `app/lib/LocalLLMClient.cpp`, `app/lib/Utils.cpp`, and 2 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
