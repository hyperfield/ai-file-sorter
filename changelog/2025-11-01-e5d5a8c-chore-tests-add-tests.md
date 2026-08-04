# 2025-11-01: chore(tests): add tests

## Covered commits
- `e5d5a8c` `2025-11-01` `chore(tests): add tests`

## Motivation
This testing commit made a specific behavior executable and checkable in automation. That kind of change reduces regression risk even when the production code difference is small.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `A` `tests/run_all_tests.sh`
- `A` `tests/run_database_tests.sh`

## What changed from what, why, and how
The commit updated test-related files in `tests/run_all_tests.sh`, `tests/run_database_tests.sh`. It changed the project from relying on implicit manual verification to having explicit automated coverage or test infrastructure for the affected behavior.

Before this commit, the repository reflected the state immediately preceding `e5d5a8c`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/tests/run_all_tests.sh b/tests/run_all_tests.sh
--- /dev/null
+++ b/tests/run_all_tests.sh
@@ -0,0 +1,27 @@
+#!/usr/bin/env bash
+set -euo pipefail
+
+ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
+
+declare -a TEST_SCRIPTS=(
+    "$ROOT_DIR/tests/run_database_tests.sh"
+    "$ROOT_DIR/tests/run_translation_tests.sh"
+)
+
+echo "Running AI File Sorter test suite"
+echo "================================="
+
+for script in "${TEST_SCRIPTS[@]}"; do
+    if [[ ! -x "$script" ]]; then
+        echo "ERROR: Test script '$script' is missing or not executable." >&2
+        exit 1
+    fi
+
+    name="$(basename "$script")"
+    echo ""
+    echo ">>> $name"
+    "$script"
+done
+
+echo ""
+echo "All tests completed successfully."
diff --git a/tests/run_database_tests.sh b/tests/run_database_tests.sh
new file mode 100755
index 0000000..749e1e8
--- /dev/null
+++ b/tests/run_database_tests.sh
@@ -0,0 +1,218 @@
+#!/usr/bin/env bash
+set -euo pipefail
+
+ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
+BUILD_DIR="$ROOT_DIR/tests/build"
+mkdir -p "$BUILD_DIR"
+mkdir -p "$BUILD_DIR/spdlog/sinks"
+mkdir -p "$BUILD_DIR/spdlog/fmt"
+
+TEST_SRC="$BUILD_DIR/database_manager_test.cpp"
+STUB_SRC="$BUILD_DIR/logger_stub.cpp"
+OUTPUT="$BUILD_DIR/database_manager_test"
+
+cat > "$TEST_SRC" <<'CPP'
+#include "DatabaseManager.hpp"
+#include "Types.hpp"
+
+#include <filesystem>
+#include <iostream>
+#include <set>
+#include <cstdlib>
+#include <chrono>
+#include <system_error>
+#include <vector>
+
+namespace {
+struct TempDir {
+    explicit TempDir(std::filesystem::path p) : path(std::move(p)) {}
+    ~TempDir() {
+        std::error_code ec;
+        std::filesystem::remove_all(path, ec);
+    }
+    std::filesystem::path path;
+};
+
+[[noreturn]] void fail(const std::string& message) {
+    std::cerr << message << '\n';
+    std::exit(1);
+}
+} // namespace
+
+int main() {
+    const auto unique_dir = std::filesystem::temp_directory_path() /
+        ("aifs-dbtest-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
+    std::filesystem::create_directories(unique_dir);
+    TempDir guard(unique_dir);
+
+    DatabaseManager manager(unique_dir.string());
+
+    const std::string test_dir = "/sample";
+    DatabaseManager::ResolvedCategory valid{0, "Docs", "Manuals"};
+    DatabaseManager::ResolvedCategory empty_cat{0, "", ""};
+    DatabaseManager::ResolvedCategory whitespace_cat{0, "   ", "   "};
+
+    if (!manager.insert_or_update_file_with_categorization("valid.txt", "F", test_dir, valid)) {
+        fail("Failed to insert valid row");
+    }
+    if (!manager.insert_or_update_file_with_categorization("empty.txt", "F", test_dir, empty_cat)) {
+        fail("Failed to insert empty row");
+    }
+    if (!manager.insert_or_update_file_with_categorization("space.txt", "F", test_dir, whitespace_cat)) {
+        fail("Failed to insert whitespace row");
+    }
```

The excerpt is taken from the commit diff for `chore(tests): add tests`. The most relevant surfaces are `tests/run_all_tests.sh`, `tests/run_database_tests.sh`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
