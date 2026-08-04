# 2025-11-20: chore(cleanup): .gitignore

## Covered commits
- `c60c1a9` `2025-11-20` `chore(cleanup): .gitignore`

## Motivation
This refactor commit improved maintainability, readability, or structure without centering the change on a brand-new feature. Those changes matter because later fixes and features depend on the code staying understandable.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `D` `.ccache/9/9/stats`
- `D` `.codacy/cli.sh`
- `D` `.codacy/codacy.yaml`
- `M` `.gitignore`

## What changed from what, why, and how
The commit reorganized or simplified code in `.ccache/9/9/stats`, `.codacy/cli.sh`, `.codacy/codacy.yaml`, `.gitignore`. It moved the repository from the earlier structure to a cleaner one so later edits would be safer and easier.

Before this commit, the repository reflected the state immediately preceding `c60c1a9`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/.ccache/9/9/stats b/.ccache/9/9/stats
--- a/.ccache/9/9/stats
+++ /dev/null
@@ -1,42 +0,0 @@
-0
-0
-0
-0
-0
-0
-0
-0
-0
-0
-0
-0
-0
-0
-0
-0
-0
-0
-0
-0
-0
-0
-0
-0
-0
-0
-0
-0
-0
-0
-0
-0
-0
-1
-0
-0
-1
-0
-0
-0
-0
-0
diff --git a/.codacy/cli.sh b/.codacy/cli.sh
deleted file mode 100755
index 7057e3b..0000000
--- a/.codacy/cli.sh
+++ /dev/null
@@ -1,149 +0,0 @@
-#!/usr/bin/env bash
-
-
-set -e +o pipefail
-
-# Set up paths first
-bin_name="codacy-cli-v2"
-
-# Determine OS-specific paths
-os_name=$(uname)
-arch=$(uname -m)
-
-case "$arch" in
-"x86_64")
-  arch="amd64"
-  ;;
-"x86")
-  arch="386"
-  ;;
-"aarch64"|"arm64")
-  arch="arm64"
-  ;;
-esac
-
-if [ -z "$CODACY_CLI_V2_TMP_FOLDER" ]; then
-    if [ "$(uname)" = "Linux" ]; then
-        CODACY_CLI_V2_TMP_FOLDER="$HOME/.cache/codacy/codacy-cli-v2"
-    elif [ "$(uname)" = "Darwin" ]; then
-        CODACY_CLI_V2_TMP_FOLDER="$HOME/Library/Caches/Codacy/codacy-cli-v2"
-    else
-        CODACY_CLI_V2_TMP_FOLDER=".codacy-cli-v2"
-    fi
-fi
-
-version_file="$CODACY_CLI_V2_TMP_FOLDER/version.yaml"
-
-
-get_version_from_yaml() {
-    if [ -f "$version_file" ]; then
-        local version=$(grep -o 'version: *"[^"]*"' "$version_file" | cut -d'"' -f2)
-        if [ -n "$version" ]; then
-            echo "$version"
-            return 0
-        fi
-    fi
-    return 1
-}
-
```

The excerpt is taken from the commit diff for `chore(cleanup): .gitignore`. The most relevant surfaces are `.ccache/9/9/stats`, `.codacy/cli.sh`, `.codacy/codacy.yaml`, `.gitignore`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
