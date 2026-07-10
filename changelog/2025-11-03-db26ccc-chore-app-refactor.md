# 2025-11-03: chore(app): refactor

## Covered commits
- `db26ccc` `2025-11-03` `chore(app): refactor`

## Motivation
This refactor commit improved maintainability, readability, or structure without centering the change on a brand-new feature. Those changes matter because later fixes and features depend on the code staying understandable.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `M` `api-key-encryption/compile.sh`
- `M` `app/include/Updater.hpp`
- `M` `app/lib/CategorizationService.cpp`
- `M` `app/lib/ConsistencyPassService.cpp`
- `M` `app/lib/CryptoManager.cpp`
- `M` `app/lib/LocalLLMClient.cpp`
- `M` `app/scripts/build_llama_windows.ps1`

## What changed from what, why, and how
The commit reorganized or simplified code in `README.md`, `api-key-encryption/compile.sh`, `app/include/Updater.hpp`, `app/lib/CategorizationService.cpp`, `app/lib/ConsistencyPassService.cpp`, `app/lib/CryptoManager.cpp`, `app/lib/LocalLLMClient.cpp`, `app/scripts/build_llama_windows.ps1`. It moved the repository from the earlier structure to a cleaner one so later edits would be safer and easier.

Before this commit, the repository reflected the state immediately preceding `db26ccc`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -221,7 +221,7 @@ Option A - CMake + vcpkg (recommended)
 1. Install prerequisites:
    - Visual Studio 2022 with Desktop C++ workload
    - CMake 3.21+ (Visual Studio ships a recent version)
-   - vcpkg: https://github.com/microsoft/vcpkg (clone and bootstrap)
+   - vcpkg: <https://github.com/microsoft/vcpkg> (clone and bootstrap)
 2. Clone repo and submodules:
    ```powershell
    git clone https://github.com/hyperfield/ai-file-sorter.git
diff --git a/api-key-encryption/compile.sh b/api-key-encryption/compile.sh
index 8e1da7d..3b69e2c 100755
--- a/api-key-encryption/compile.sh
+++ b/api-key-encryption/compile.sh
@@ -1 +1,3 @@
-g++ obfuscate_encrypt.cpp -Wall -o obfuscate_encrypt -lssl -lcrypto
\ No newline at end of file
+#!/usr/bin/env bash
+
+g++ obfuscate_encrypt.cpp -Wall -o obfuscate_encrypt -lssl -lcrypto
```

The excerpt is taken from the commit diff for `chore(app): refactor`. The most relevant surfaces are `README.md`, `api-key-encryption/compile.sh`, `app/include/Updater.hpp`, `app/lib/CategorizationService.cpp`, `app/lib/ConsistencyPassService.cpp`, and 3 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
