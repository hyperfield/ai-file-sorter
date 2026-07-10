# 2025-07-13: feat(utils): improve is_network_available() for Windows

## Covered commits
- `536066d` `2025-07-13` `feat(utils): improve is_network_available() for Windows`

## Motivation
This feature commit introduced a new user-facing or architecture-facing capability that the earlier codebase did not yet provide. The motivation was to expand the supported workflow rather than merely tune the existing one.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/Utils.cpp`

## What changed from what, why, and how
The commit added or exposed new functionality in `app/lib/Utils.cpp`. It changed the project from not having the capability described by `feat(utils): improve is_network_available() for Windows` to having a concrete implementation of it.

Before this commit, the repository reflected the state immediately preceding `536066d`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/Utils.cpp b/app/lib/Utils.cpp
--- a/app/lib/Utils.cpp
+++ b/app/lib/Utils.cpp
@@ -6,6 +6,7 @@
 #include <glibmm/fileutils.h>
 #ifdef _WIN32
     #include <windows.h>
+    #include <wininet.h>
 #elif __linux__
     #include <dlfcn.h>
     #include <limits.h>
@@ -67,11 +68,12 @@ constexpr cl_uint CL_DEVICE_NAME = 0x102B;
 bool Utils::is_network_available()
 {
 #ifdef _WIN32
-    int result = system("ping -n 1 google.com > NUL 2>&1");
+    DWORD flags;
+    return InternetGetConnectedState(&flags, 0);
 #else
     int result = system("ping -c 1 google.com > /dev/null 2>&1");
-#endif
     return result == 0;
+#endif
 }
```

The excerpt is taken from the commit diff for `feat(utils): improve is_network_available() for Windows`. The most relevant surfaces are `app/lib/Utils.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
