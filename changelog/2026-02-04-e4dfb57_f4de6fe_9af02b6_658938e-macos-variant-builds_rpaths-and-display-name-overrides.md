# 2026-02-04: macOS variant builds, cross-compile guards, rpaths, and display-name overrides

## Covered commits
- `e4dfb57` `2026-02-04` `build(macos): add variant targets and x86_64 cross-compile fixes`
- `f4de6fe` `2026-02-04` `build(llama): add macOS arch flags and cross-compile guards`
- `9af02b6` `2026-02-04` `build(deps): add macOS x64 pdfium vendor support`
- `658938e` `2026-02-04` `feat(build): add per-variant llama outputs and rpaths`
- `308130f` `2026-02-04` `feat(ui): support app display name overrides on macOS`
- `fa97688` `2026-02-04` `docs(readme): clarify macOS build variants`
- `0f4aafb` `2026-02-04` `docs: update macOS build targets and changelog`

## Motivation
The macOS build had outgrown the single-binary assumption. Supporting Apple Silicon and Intel cleanly required architecture-specific variant targets, explicit cross-compile guards, vendor support for macOS x64 PDFium, and runtime search paths that would keep bundled artifacts isolated from system libraries.

## What changed
These commits added macOS variant targets, arch flags and cross-compile safeguards for llama.cpp, macOS x64 PDFium vendor support, per-variant runtime outputs and rpaths, app display-name overrides, and corresponding README/changelog updates for builders.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `e4dfb57`
```diff
diff --git a/app/Makefile b/app/Makefile
--- a/app/Makefile
+++ b/app/Makefile
@@ -5,6 +5,11 @@ UNAME_M := $(shell uname -m)
 BIN_DIR := ./bin
 OBJ_DIR := ./obj
 SRC_DIR := ./lib
+APP_NAME ?= AI File Sorter
+
+# Optional: build llama.cpp with multi-variant CPU backends on macOS.
+# Usage: make LLAMA_MULTI_VARIANT=1
+LLAMA_MULTI_VARIANT ?= 0
 
 ifeq ($(UNAME), Linux)
     PLATFORM := Linux
@@ -40,9 +45,15 @@ endif
 	LDFLAGS += -Wl,-rpath-link=./lib/precompiled/cpu/bin -Wl,-rpath-link=./lib/precompiled
 
 else ifeq ($(UNAME), Darwin)
-    IS_APPLE_SILICON := $(shell sysctl -n machdep.cpu.brand_string | grep -i "Apple" > /dev/null && echo 1 || echo 0)
-    DEFAULT_BREW_PREFIX := $(shell if [ "$(IS_APPLE_SILICON)" = "1" ]; then echo /opt/homebrew; else echo /usr/local; fi)
-    BREW_PREFIX := $(shell if command -v brew >/dev/null 2>&1; then brew --prefix; else echo $(DEFAULT_BREW_PREFIX); fi)
+    MACOS_ARCH ?= $(UNAME_M)
+    DEFAULT_BREW_PREFIX := $(shell if [ "$(MACOS_ARCH)" = "arm64" ]; then echo /opt/homebrew; else echo /usr/local; fi)
+ifeq ($(origin BREW_PREFIX),undefined)
+    ifeq ($(MACOS_ARCH),x86_64)
+        BREW_PREFIX := /usr/local
+    else
+        BREW_PREFIX := $(shell if command -v brew >/dev/null 2>&1; then brew --prefix; else echo $(DEFAULT_BREW_PREFIX); fi)
+    endif
+endif
     QT_PREFIX := $(BREW_PREFIX)/opt/qt
     QTBASE_PREFIX := $(BREW_PREFIX)/opt/qtbase
     CURL_PREFIX := $(BREW_PREFIX)/opt/curl
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `f4de6fe`
```diff
diff --git a/app/scripts/build_llama_macos.sh b/app/scripts/build_llama_macos.sh
--- a/app/scripts/build_llama_macos.sh
+++ b/app/scripts/build_llama_macos.sh
@@ -1,6 +1,69 @@
 #!/bin/bash
 set -e
 
+# CLI flags
+usage() {
+    cat <<'USAGE'
+Usage: build_llama_macos.sh [options]
+
+Options:
+  --arch <arm64|x86_64>   Target macOS architecture
+  --arm64                 Alias for --arch arm64 (Apple Silicon)
+  --x86_64                Alias for --arch x86_64 (Intel)
+  --intel                 Alias for --arch x86_64
+  --m1 | --m2 | --m3      Alias for --arch arm64
+  -h | --help             Show this help
+
+Environment overrides:
+  LLAMA_MACOS_ARCH        Target architecture (overridden by CLI)
+  LLAMA_MACOS_ENABLE_METAL=0|1|auto
+  LLAMA_MACOS_MULTI_VARIANT=0|1
+USAGE
+}
+
+CLI_ARCH=""
+while [[ $# -gt 0 ]]; do
+    case "$1" in
+        --arch)
+            if [[ -z "${2:-}" ]]; then
+                echo "Missing value for --arch" >&2
+                usage
+                exit 1
+            fi
+            CLI_ARCH="$2"
+            shift 2
+            ;;
+        --arm64|--apple-silicon|--m1|--m2|--m3)
+            CLI_ARCH="arm64"
+            shift
+            ;;
+        --x86_64|--intel)
+            CLI_ARCH="x86_64"
+            shift
+            ;;
+        -h|--help)
+            usage
+            exit 0
+            ;;
+        *)
+            echo "Unknown option: $1" >&2
+            usage
+            exit 1
+            ;;
+    esac
+done
+
+if [[ -n "$CLI_ARCH" ]]; then
+    case "$CLI_ARCH" in
+        arm64|x86_64) ;;
+        *)
+            echo "Unsupported arch: $CLI_ARCH (use arm64 or x86_64)" >&2
+            exit 1
+            ;;
+    esac
+    export LLAMA_MACOS_ARCH="$CLI_ARCH"
+fi
+
 # Resolve script directory (cross-shell portable)
 SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
 LLAMA_DIR="$SCRIPT_DIR/../include/external/llama.cpp"
@@ -15,7 +78,12 @@ PRECOMPILED_LIBS_DIR="$SCRIPT_DIR/../lib/precompiled"
 HEADERS_DIR="$SCRIPT_DIR/../include/llama"
 
 ARCH=$(uname -m)
-echo "Building on architecture: $ARCH"
+TARGET_ARCH=${LLAMA_MACOS_ARCH:-$ARCH}
+echo "Building on architecture: $ARCH (target: $TARGET_ARCH)"
```

This second excerpt is included because `f4de6fe` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
