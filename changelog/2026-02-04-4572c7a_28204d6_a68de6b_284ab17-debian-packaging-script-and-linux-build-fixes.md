# 2026-02-04: Debian packaging script and Linux build fixes

## Covered commits
- `4572c7a` `2026-02-04` `feat(scripts): add deb creation script`
- `28204d6` `2026-02-04` `fix(scripts): deb script`
- `a68de6b` `2026-02-04` `fix(scripts): deb script`
- `284ab17` `2026-02-04` `fix(linux-compile): debian`
- `c426c32` `2026-02-05` `docs(readme): add zlib build note`

## Motivation
Linux distribution became a first-class concern once the app accumulated more vendored and staged runtime pieces. A dedicated Debian package script was the practical way to capture that deployment shape, but it immediately required follow-up fixes and Linux build repairs to work on real systems.

## What changed
The grouped commits introduced the Debian package-creation script, fixed the script in follow-up commits, repaired Debian/Linux compilation problems, and documented the extra zlib/build note needed for the resulting packaging flow.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `4572c7a`
```diff
diff --git a/app/scripts/package_deb.sh b/app/scripts/package_deb.sh
--- /dev/null
+++ b/app/scripts/package_deb.sh
@@ -0,0 +1,174 @@
+#!/usr/bin/env bash
+
+set -euo pipefail
+
+# Builds a Debian package for AI File Sorter that bundles only the project-specific
+# llama/ggml libraries and assumes all other runtime libraries are supplied by the system.
+#
+# Usage:
+#   ./package_deb.sh [version]
+# If no version is supplied, the script reads app/include/app_version.hpp.
+
+SCRIPT_DIR="$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
+REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"
+APP_DIR="$REPO_ROOT/app"
+
+VERSION_FROM_HEADER() {
+    local header="$1"
+    if [[ ! -f "$header" ]]; then
+        echo "0.0.0"
+        return
+    fi
+    local line
+    line="$(grep -m1 'APP_VERSION' "$header" || true)"
+    if [[ -z "$line" ]]; then
+        echo "0.0.0"
+        return
+    fi
+    if [[ "$line" =~ Version\{[[:space:]]*([0-9]+)[[:space:]]*,[[:space:]]*([0-9]+)[[:space:]]*,[[:space:]]*([0-9]+)[[:space:]]*\} ]]; then
+        printf "%s.%s.%s\n" "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}" "${BASH_REMATCH[3]}"
+    else
+        echo "0.0.0"
+    fi
+}
+
+VERSION="${1:-$(VERSION_FROM_HEADER "$APP_DIR/include/app_version.hpp")}"
+
+if [[ -z "$VERSION" ]]; then
+    echo "Failed to determine package version." >&2
+    exit 1
+fi
+
+BIN_PATH="$APP_DIR/bin/aifilesorter"
+if [[ ! -x "$BIN_PATH" ]]; then
+    echo "Binary not found at $BIN_PATH — running make." >&2
+    make -C "$APP_DIR"
+fi
+
+if [[ ! -x "$BIN_PATH" ]]; then
+    echo "Binary still missing after build attempt." >&2
+    exit 1
+fi
+
+OUT_DIR="$REPO_ROOT/dist/aifilesorter_deb"
+PKG_NAME="aifilesorter_${VERSION}"
+PKG_ROOT="$OUT_DIR/$PKG_NAME"
+
+echo "Staging package in $PKG_ROOT"
+rm -rf "$PKG_ROOT"
+mkdir -p \
+    "$PKG_ROOT/DEBIAN" \
+    "$PKG_ROOT/opt/aifilesorter/bin" \
+    "$PKG_ROOT/opt/aifilesorter/lib" \
+    "$PKG_ROOT/opt/aifilesorter/certs" \
+    "$PKG_ROOT/usr/bin"
+
+install -m 0755 "$BIN_PATH" "$PKG_ROOT/opt/aifilesorter/bin/aifilesorter-bin"
+ln -sf aifilesorter-bin "$PKG_ROOT/opt/aifilesorter/bin/aifilesorter"
+
+echo "Copying llama/ggml libraries"
+if [[ -d "$APP_DIR/lib/precompiled" ]]; then
+    cp -a "$APP_DIR/lib/precompiled" "$PKG_ROOT/opt/aifilesorter/lib/"
+fi
+
+if [[ -f "$APP_DIR/resources/certs/cacert.pem" ]]; then
+    install -m 0644 "$APP_DIR/resources/certs/cacert.pem" "$PKG_ROOT/opt/aifilesorter/certs/cacert.pem"
+fi
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `28204d6`
```diff
diff --git a/app/include/external/llama.cpp b/app/include/external/llama.cpp
--- a/app/include/external/llama.cpp
+++ b/app/include/external/llama.cpp
@@ -1 +1 @@
-Subproject commit 1aa7c497c5e1929771c4fc3fc8e37c7157fa9bbd
+Subproject commit a89002f07b55dace8671fc07b2e2418700716992
diff --git a/app/scripts/package_deb.sh b/app/scripts/package_deb.sh
index 4e02b19..5f91edd 100755
--- a/app/scripts/package_deb.sh
+++ b/app/scripts/package_deb.sh
@@ -39,7 +39,7 @@ if [[ -z "$VERSION" ]]; then
     exit 1
 fi
 
-BIN_PATH="$APP_DIR/bin/aifilesorter"
+BIN_PATH="$APP_DIR/bin/aifilesorter-bin"
 if [[ ! -x "$BIN_PATH" ]]; then
     echo "Binary not found at $BIN_PATH — running make." >&2
     make -C "$APP_DIR"
```

This second excerpt is included because `28204d6` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
