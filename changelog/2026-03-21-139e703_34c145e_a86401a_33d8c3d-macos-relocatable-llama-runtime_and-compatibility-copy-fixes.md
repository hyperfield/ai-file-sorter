# 2026-03-21: Relocatable macOS llama runtime and compatibility copy fixes

## Covered commits
- `139e703` `2026-03-21` `fix(macos-build): make llama dylibs relocatable`
- `34c145e` `2026-03-21` `fix(macos-runtime): avoid system ggml collisions`
- `a86401a` `2026-03-21` `Fix spelling of 'Compatibility' in translations`
- `33d8c3d` `2026-03-21` `Fix typo in window title from 'Compatability' to 'Compatibility'`

## Motivation
The macOS runtime still had a packaging correctness problem: bundled llama/ggml dylibs needed to be relocatable and preferred over conflicting system/Homebrew installations. While touching that surface, the UI compatibility text also got small but worthwhile correctness fixes.

## What changed
This grouped chapter covers the relocatable-dylib build change, the runtime hardening that avoids system ggml collisions, and the spelling/copy fixes that changed “Compatability” to “Compatibility” in the UI and translations.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `139e703`
```diff
diff --git a/app/scripts/build_llama_macos.sh b/app/scripts/build_llama_macos.sh
--- a/app/scripts/build_llama_macos.sh
+++ b/app/scripts/build_llama_macos.sh
@@ -79,6 +79,26 @@ if [[ "$PRECOMPILED_LIBS_DIR" != /* ]]; then
     PRECOMPILED_LIBS_DIR="$SCRIPT_DIR/../$PRECOMPILED_LIBS_DIR"
 fi
 HEADERS_DIR="$SCRIPT_DIR/../include/llama"
+BUILD_DIR="$LLAMA_DIR/build"
+BUILD_BIN_DIR="$BUILD_DIR/bin"
+DYLIB_RPATH="@loader_path"
+
+normalize_macos_dylib_rpaths() {
+    local dylib_dir="$1"
+    local stale_rpath="$2"
+
+    shopt -s nullglob
+    for dylib in "$dylib_dir"/*.dylib; do
+        if ! otool -l "$dylib" | grep -Fq "$DYLIB_RPATH"; then
+            install_name_tool -add_rpath "$DYLIB_RPATH" "$dylib"
+        fi
+
+        if otool -l "$dylib" | grep -Fq "$stale_rpath"; then
+            install_name_tool -delete_rpath "$stale_rpath" "$dylib"
+        fi
+    done
+    shopt -u nullglob
+}
 
 ARCH=$(uname -m)
 TARGET_ARCH=${LLAMA_MACOS_ARCH:-$ARCH}
@@ -173,13 +193,15 @@ fi
 
 # Enter llama.cpp directory and build
 cd "$LLAMA_DIR"
-rm -rf build
-mkdir -p build
-LDFLAGS= cmake -S . -B build \
+rm -rf "$BUILD_DIR"
+mkdir -p "$BUILD_DIR"
+LDFLAGS= cmake -S . -B "$BUILD_DIR" \
   ${CMAKE_SYSROOT_ARG} \
   ${ARCH_CMAKE_ARG} \
   -DCMAKE_BUILD_TYPE=Release \
   -DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET} \
+  -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON \
+  -DCMAKE_INSTALL_RPATH=${DYLIB_RPATH} \
   -DOPENSSL_ROOT_DIR=${OPENSSL_PREFIX} \
   -DBUILD_SHARED_LIBS=ON \
   -DCMAKE_EXE_LINKER_FLAGS= \
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `34c145e`
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -414,6 +414,8 @@ File categorization with local LLMs is completely free of charge. If you prefer
    ```bash
    ./app/scripts/build_llama_macos.sh
    ```
+   The macOS app and `.app` bundles use the runtime staged under `app/lib/precompiled*`; they do not need Homebrew `ggml` or `llama.cpp` libraries.
+   If you have older `ggml` / `llama.cpp` copies installed in generic library locations, prefer unlinking or removing them instead of relying on them implicitly.
 7. **Compile the application**
 
    ```bash
@@ -434,6 +436,7 @@ File categorization with local LLMs is completely free of charge. If you prefer
 
    These targets rebuild the llama.cpp runtime before compiling the app.
    When cross-compiling Intel on Apple Silicon, use x86_64 Homebrew (under `/usr/local`) or set `BREW_PREFIX=/usr/local` so Qt/pkg-config resolve correctly.
+   `sudo make install` places the macOS runtime libraries under `/usr/local/lib/aifilesorter` to avoid collisions with unrelated system or Homebrew ggml libraries.
    Each variant uses distinct build directories to avoid cross-arch collisions:
    - llama.cpp libs: `app/lib/precompiled-m1`, `app/lib/precompiled-m2`, `app/lib/precompiled-intel`
    - object files: `app/obj/arm64` or `app/obj/x86_64`
```

This second excerpt is included because `34c145e` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
