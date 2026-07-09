# 2025-11-01: fix(core): utf-8 encoding issue with paths

## Covered commits
- `4aa5d8c` `2025-11-01` `fix(core): utf-8 encoding issue with paths`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/Utils.hpp`
- `M` `app/include/external/llama.cpp`
- `M` `app/lib/FileScanner.cpp`
- `M` `app/lib/MainApp.cpp`
- `M` `app/lib/MovableCategorizedFile.cpp`
- `M` `app/lib/Settings.cpp`
- `M` `app/lib/Utils.cpp`

## What changed from what, why, and how
The commit corrected behavior in `app/include/Utils.hpp`, `app/include/external/llama.cpp`, `app/lib/FileScanner.cpp`, `app/lib/MainApp.cpp`, `app/lib/MovableCategorizedFile.cpp`, `app/lib/Settings.cpp`, `app/lib/Utils.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(core): utf-8 encoding issue with paths`.

Before this commit, the repository reflected the state immediately preceding `4aa5d8c`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/Utils.hpp b/app/include/Utils.hpp
--- a/app/include/Utils.hpp
+++ b/app/include/Utils.hpp
@@ -44,6 +44,8 @@ public:
     static std::string get_cudart_dll_name();
     static std::string abbreviate_user_path(const std::string& path);
     static std::filesystem::path ensure_ca_bundle();
+    static std::string path_to_utf8(const std::filesystem::path& path);
+    static std::filesystem::path utf8_to_path(const std::string& utf8_path);
 
 private:
     static int get_ngl(int vram_mb);
diff --git a/app/include/external/llama.cpp b/app/include/external/llama.cpp
index 3cfa9c3..7675c55 160000
--- a/app/include/external/llama.cpp
+++ b/app/include/external/llama.cpp
@@ -1 +1 @@
-Subproject commit 3cfa9c3f125763305b4226bc032f1954f08990dc
+Subproject commit 7675c555a13c9f473249e59a54db35032ce8e0fc
```

The excerpt is taken from the commit diff for `fix(core): utf-8 encoding issue with paths`. The most relevant surfaces are `app/include/Utils.hpp`, `app/include/external/llama.cpp`, `app/lib/FileScanner.cpp`, `app/lib/MainApp.cpp`, `app/lib/MovableCategorizedFile.cpp`, and 2 more file(s); those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
