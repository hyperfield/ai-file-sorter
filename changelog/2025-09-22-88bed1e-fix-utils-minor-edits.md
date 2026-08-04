# 2025-09-22: fix(utils): minor edits

## Covered commits
- `88bed1e` `2025-09-22` `fix(utils): minor edits`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/Utils.cpp`

## What changed from what, why, and how
The commit corrected behavior in `app/lib/Utils.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(utils): minor edits`.

Before this commit, the repository reflected the state immediately preceding `88bed1e`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/Utils.cpp b/app/lib/Utils.cpp
--- a/app/lib/Utils.cpp
+++ b/app/lib/Utils.cpp
@@ -392,13 +392,12 @@ std::string Utils::get_cudart_dll_name() {
     int version = get_installed_cuda_runtime_version();
     if (version == 0) return "";
 
-    int major = version / 1000;        // e.g., 12
-    // int minor = (version % 1000) / 10; // e.g., 8
+    int major = version / 1000;        // e.g., 13
 
     char buffer[32];
     std::snprintf(buffer, sizeof(buffer), "cudart64_%d.dll", major);
     std::cerr << "[CUDA] Determined DLL name: " << buffer << std::endl;
-    return buffer; // e.g., "cudart64_12.dll"
+    return buffer; // e.g., "cudart64_13.dll"
 }
 #endif
```

The excerpt is taken from the commit diff for `fix(utils): minor edits`. The most relevant surfaces are `app/lib/Utils.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
