# 2025-11-06: feat(taxonomy): consistency pass r&d

## Covered commits
- `1e928ef` `2025-11-06` `feat(taxonomy): consistency pass r&d`

## Motivation
This feature commit introduced a new user-facing or architecture-facing capability that the earlier codebase did not yet provide. The motivation was to expand the supported workflow rather than merely tune the existing one.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `A` `.codacy/cli.sh`
- `A` `.codacy/codacy.yaml`
- `M` `app/include/LocalLLMClient.hpp`
- `M` `app/lib/ConsistencyPassService.cpp`
- `M` `app/lib/LocalLLMClient.cpp`
- `M` `app/lib/MainApp.cpp`
- `A` `prototypes/constrained_taxonomy/README.md`
- `A` `prototypes/constrained_taxonomy/TaxonomyTemplatePrototype.cpp`

## What changed from what, why, and how
The commit added or exposed new functionality in `.codacy/cli.sh`, `.codacy/codacy.yaml`, `app/include/LocalLLMClient.hpp`, `app/lib/ConsistencyPassService.cpp`, `app/lib/LocalLLMClient.cpp`, `app/lib/MainApp.cpp`, `prototypes/constrained_taxonomy/README.md`, `prototypes/constrained_taxonomy/TaxonomyTemplatePrototype.cpp`. It changed the project from not having the capability described by `feat(taxonomy): consistency pass r&d` to having a concrete implementation of it.

Before this commit, the repository reflected the state immediately preceding `1e928ef`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/.codacy/cli.sh b/.codacy/cli.sh
--- /dev/null
+++ b/.codacy/cli.sh
@@ -0,0 +1,149 @@
+#!/usr/bin/env bash
+
+
+set -e +o pipefail
+
+# Set up paths first
+bin_name="codacy-cli-v2"
+
+# Determine OS-specific paths
+os_name=$(uname)
+arch=$(uname -m)
+
+case "$arch" in
+"x86_64")
+  arch="amd64"
+  ;;
+"x86")
+  arch="386"
+  ;;
+"aarch64"|"arm64")
+  arch="arm64"
+  ;;
+esac
+
+if [ -z "$CODACY_CLI_V2_TMP_FOLDER" ]; then
+    if [ "$(uname)" = "Linux" ]; then
+        CODACY_CLI_V2_TMP_FOLDER="$HOME/.cache/codacy/codacy-cli-v2"
+    elif [ "$(uname)" = "Darwin" ]; then
+        CODACY_CLI_V2_TMP_FOLDER="$HOME/Library/Caches/Codacy/codacy-cli-v2"
+    else
+        CODACY_CLI_V2_TMP_FOLDER=".codacy-cli-v2"
+    fi
+fi
+
+version_file="$CODACY_CLI_V2_TMP_FOLDER/version.yaml"
+
+
+get_version_from_yaml() {
+    if [ -f "$version_file" ]; then
+        local version=$(grep -o 'version: *"[^"]*"' "$version_file" | cut -d'"' -f2)
+        if [ -n "$version" ]; then
+            echo "$version"
+            return 0
+        fi
+    fi
+    return 1
+}
+
+get_latest_version() {
+    local response
+    if [ -n "$GH_TOKEN" ]; then
+        response=$(curl -Lq --header "Authorization: Bearer $GH_TOKEN" "https://api.github.com/repos/codacy/codacy-cli-v2/releases/latest" 2>/dev/null)
+    else
+        response=$(curl -Lq "https://api.github.com/repos/codacy/codacy-cli-v2/releases/latest" 2>/dev/null)
+    fi
+
+    handle_rate_limit "$response"
+    local version=$(echo "$response" | grep -m 1 tag_name | cut -d'"' -f4)
+    echo "$version"
+}
+
+handle_rate_limit() {
+    local response="$1"
+    if echo "$response" | grep -q "API rate limit exceeded"; then
+          fatal "Error: GitHub API rate limit exceeded. Please try again later"
+    fi
+}
+
+download_file() {
+    local url="$1"
+
+    echo "Downloading from URL: ${url}"
+    if command -v curl > /dev/null 2>&1; then
+        curl -# -LS "$url" -O
+    elif command -v wget > /dev/null 2>&1; then
+        wget "$url"
+    else
+        fatal "Error: Could not find curl or wget, please install one."
+    fi
+}
+
+download() {
+    local url="$1"
+    local output_folder="$2"
+
+    ( cd "$output_folder" && download_file "$url" )
+}
+
+download_cli() {
+    # OS name lower case
+    suffix=$(echo "$os_name" | tr '[:upper:]' '[:lower:]')
+
+    local bin_folder="$1"
+    local bin_path="$2"
+    local version="$3"
+
```

The excerpt is taken from the commit diff for `feat(taxonomy): consistency pass r&d`. The most relevant surfaces are `.codacy/cli.sh`, `.codacy/codacy.yaml`, `app/include/LocalLLMClient.hpp`, `app/lib/ConsistencyPassService.cpp`, `app/lib/LocalLLMClient.cpp`, and 3 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
