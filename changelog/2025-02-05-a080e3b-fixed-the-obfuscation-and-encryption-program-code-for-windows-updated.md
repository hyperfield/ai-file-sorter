# 2025-02-05: Fixed the obfuscation and encryption program code for Windows. Updated the README

## Covered commits
- `a080e3b` `2025-02-05` `Fixed the obfuscation and encryption program code for Windows. Updated the README`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`
- `M` `api-key-encryption/obfuscate_encrypt.cpp`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`, `api-key-encryption/obfuscate_encrypt.cpp`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `a080e3b`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -164,7 +164,11 @@ Before compiling the app:
 
 2. Generate a 32-character random secret key, e.g., using [this tool](https://passwords-generator.org/32-character).
 
-3. Navigate to the `api-key-encryption` folder and edit the `encryption.ini` as follows:
+    **Important**: If you're compiling on Windows, make sure there is NO `=` in the generated key! If one or more `=` are there, regenerate the key!
+
+    Your secret key could look something like `du)]--Wg#+Au89Ro6eRMJc"]qx~owL_X`.
+
+3. Navigate to the `api-key-encryption` folder, then make a file named `encryption.ini` with the following content:
 
     ```
     LLM_API_KEY=sk-...
diff --git a/api-key-encryption/obfuscate_encrypt.cpp b/api-key-encryption/obfuscate_encrypt.cpp
index 57c964c..947f6a7 100644
--- a/api-key-encryption/obfuscate_encrypt.cpp
+++ b/api-key-encryption/obfuscate_encrypt.cpp
@@ -2,6 +2,17 @@
 #include <stddef.h>
 #include <string>
 #include <cstring>
+
+#ifdef _WIN32
+#include <cstdlib>
+
+// Need setenv() before including dotenv.h on Windows
+int setenv(const char *name, const char *value, int overwrite) {
+    if (!overwrite && getenv(name)) return 0;
+    return _putenv_s(name, value);
+}
+#endif
+
 #include "../app/include/external/dotenv.h"
 #include <stdexcept>
 #include <iostream>
```

The excerpt is taken from the commit diff for `Fixed the obfuscation and encryption program code for Windows. Updated the README`. The most relevant surfaces are `README.md`, `api-key-encryption/obfuscate_encrypt.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
