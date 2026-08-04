# 2026-03-09: Cross-platform PDFium staging, macOS/Homebrew arch fixes, Linux clean-target fixes, and Debian variant packaging

## Covered commits
- `9ec0b1b` `2026-03-09` `fix(build): use arch-specific Homebrew config on macOS`
- `b2e5baf` `2026-03-09` `fix(build): use arch-specific Homebrew paths for macOS llama builds`
- `265f3ff` `2026-03-09` `fix(build): auto-stage pdfium for macOS app builds`
- `cfbee5c` `2026-03-09` `fix(macos): accept the MTL backend alias for Metal`
- `b6749f7` `2026-03-09` `fix(build): allow clean target without dependency discovery`
- `0697011` `2026-03-10` `fix(build): stage Linux PDFium runtime and expose precompiled root in launcher`
- `e5f68d5` `2026-03-10` `feat(packaging): add deb variant flags for cpu/cuda/vulkan precompiled payloads`
- `ad1535f` `2026-03-09` `chore(deps): pin llama.cpp submodule to ae9f8df`
- `5302d85` `2026-03-10` `chore(deps): bump llama.cpp to 59db9a3`
- `cd72c57` `2026-03-09` `chore(deps): bump llama.cpp to d417bc4`
- `57638cb` `2026-03-09` `chore(gitignore): ignore generated precompiled runtime dirs`

## Motivation
Packaging and runtime staging had become the main deployment risk. The app needed to stage PDFium artifacts correctly on macOS and Linux, respect architecture-specific Homebrew paths, keep Metal backend aliases working, allow clean builds without pre-discovered dependencies, and finally emit Debian packages that could include different precompiled runtime variants.

## What changed
This grouped build-and-packaging chapter captures the PDFium staging work, macOS/Homebrew arch-path fixes, Metal alias acceptance, clean-target reliability, generated-runtime ignore rules, llama.cpp submodule pin updates that supported the packaging flow, and the Debian package variant flags for CPU/CUDA/Vulkan payloads.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `9ec0b1b`
```diff
diff --git a/app/Makefile b/app/Makefile
--- a/app/Makefile
+++ b/app/Makefile
@@ -62,17 +62,20 @@ endif
     EXPAT_PREFIX := $(BREW_PREFIX)/opt/expat
     SDKROOT := $(shell xcrun --sdk macosx --show-sdk-path 2>/dev/null)
     MACOSX_DEPLOYMENT_TARGET ?= 11.0
+    MACOS_PKG_CONFIG_DIRS := $(BREW_PREFIX)/lib/pkgconfig:$(BREW_PREFIX)/share/pkgconfig:$(LIBFFI_PREFIX)/lib/pkgconfig:$(EXPAT_PREFIX)/lib/pkgconfig:$(QT_PREFIX)/lib/pkgconfig
 
     export MACOSX_DEPLOYMENT_TARGET
     PATH := $(QTBASE_PREFIX)/share/qt/libexec:$(QT_PREFIX)/bin:$(CURL_PREFIX)/bin:$(PATH)
     export PATH
 
 ifeq ($(MACOS_ARCH),x86_64)
-    export PKG_CONFIG_PATH := $(BREW_PREFIX)/lib/pkgconfig:$(BREW_PREFIX)/share/pkgconfig:$(LIBFFI_PREFIX)/lib/pkgconfig:$(EXPAT_PREFIX)/lib/pkgconfig:$(QT_PREFIX)/lib/pkgconfig
+    export PKG_CONFIG_PATH := $(MACOS_PKG_CONFIG_DIRS)
+    export PKG_CONFIG_LIBDIR := $(MACOS_PKG_CONFIG_DIRS)
     CPPFLAGS := $(filter-out -I/opt/homebrew/%,$(CPPFLAGS))
     LDFLAGS := $(filter-out -L/opt/homebrew/% -F/opt/homebrew/%,$(LDFLAGS))
 else
-    export PKG_CONFIG_PATH := $(BREW_PREFIX)/lib/pkgconfig:$(BREW_PREFIX)/share/pkgconfig:$(LIBFFI_PREFIX)/lib/pkgconfig:$(EXPAT_PREFIX)/lib/pkgconfig:$(QT_PREFIX)/lib/pkgconfig:$(PKG_CONFIG_PATH)
+    export PKG_CONFIG_PATH := $(MACOS_PKG_CONFIG_DIRS):$(PKG_CONFIG_PATH)
+    export PKG_CONFIG_LIBDIR := $(MACOS_PKG_CONFIG_DIRS)
 endif
 
     export LDFLAGS += -L$(LIBFFI_PREFIX)/lib
@@ -233,6 +236,8 @@ PUGIXML_HDR := $(PUGIXML_DIR)/src/pugixml.hpp
 PDFIUM_PLATFORM_DIR :=
 PDFIUM_INC :=
 PDFIUM_LIB :=
+PDFIUM_STAGED_LIB :=
+PDFIUM_STAGED_STAMP :=
 ifeq ($(UNAME), Linux)
 PDFIUM_PLATFORM_DIR := $(PDFIUM_DIR)/linux-x64
 PDFIUM_INC := $(PDFIUM_PLATFORM_DIR)/include
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `b2e5baf`
```diff
diff --git a/app/scripts/build_llama_macos.sh b/app/scripts/build_llama_macos.sh
--- a/app/scripts/build_llama_macos.sh
+++ b/app/scripts/build_llama_macos.sh
@@ -92,6 +92,37 @@ MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET:-11.0}
 export MACOSX_DEPLOYMENT_TARGET
 echo "Targeting macOS ${MACOSX_DEPLOYMENT_TARGET} for build outputs"
 
+DEFAULT_BREW_PREFIX="/opt/homebrew"
+if [ "$TARGET_ARCH" = "x86_64" ]; then
+    DEFAULT_BREW_PREFIX="/usr/local"
+fi
+if [ -n "${BREW_PREFIX:-}" ]; then
+    HOMEBREW_PREFIX="$BREW_PREFIX"
+elif [ -x "${DEFAULT_BREW_PREFIX}/bin/brew" ]; then
+    HOMEBREW_PREFIX="$(${DEFAULT_BREW_PREFIX}/bin/brew --prefix)"
+elif command -v brew >/dev/null 2>&1; then
+    HOMEBREW_PREFIX="$(brew --prefix)"
+else
+    HOMEBREW_PREFIX="$DEFAULT_BREW_PREFIX"
+fi
+echo "Using Homebrew prefix: ${HOMEBREW_PREFIX}"
+
+QT_PREFIX="${HOMEBREW_PREFIX}/opt/qt"
+OPENSSL_PREFIX="${HOMEBREW_PREFIX}/opt/openssl@3"
+PKG_CONFIG_DIRS=(
+    "${HOMEBREW_PREFIX}/lib/pkgconfig"
+    "${HOMEBREW_PREFIX}/share/pkgconfig"
+    "${OPENSSL_PREFIX}/lib/pkgconfig"
+    "${QT_PREFIX}/lib/pkgconfig"
+)
+PKG_CONFIG_PATH_VALUE="$(IFS=:; echo "${PKG_CONFIG_DIRS[*]}")"
+export PKG_CONFIG_PATH="${PKG_CONFIG_PATH_VALUE}${PKG_CONFIG_PATH:+:${PKG_CONFIG_PATH}}"
+export PKG_CONFIG_LIBDIR="${PKG_CONFIG_PATH_VALUE}"
+export CMAKE_PREFIX_PATH="${HOMEBREW_PREFIX}${CMAKE_PREFIX_PATH:+;${CMAKE_PREFIX_PATH}}"
+if [ "$TARGET_ARCH" = "x86_64" ]; then
+    export CMAKE_IGNORE_PREFIX_PATH="/opt/homebrew${CMAKE_IGNORE_PREFIX_PATH:+;${CMAKE_IGNORE_PREFIX_PATH}}"
+fi
+
 # Decide whether to enable Metal. Apple Silicon machines benefit from it, but
 # most Intel Macs either lack usable Metal compute queues or expose 0 bytes of
 # GPU memory to ggml, which causes llama.cpp to fail the moment it tries to
@@ -149,6 +180,7 @@ LDFLAGS= cmake -S . -B build \
   ${ARCH_CMAKE_ARG} \
   -DCMAKE_BUILD_TYPE=Release \
   -DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET} \
+  -DOPENSSL_ROOT_DIR=${OPENSSL_PREFIX} \
   -DBUILD_SHARED_LIBS=ON \
   -DCMAKE_EXE_LINKER_FLAGS= \
   -DCMAKE_SHARED_LINKER_FLAGS= \
```

This second excerpt is included because `b2e5baf` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
