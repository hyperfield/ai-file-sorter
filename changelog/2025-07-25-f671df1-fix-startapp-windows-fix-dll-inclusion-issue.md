# 2025-07-25: fix(startapp-windows): fix dll inclusion issue

## Covered commits
- `f671df1` `2025-07-25` `fix(startapp-windows): fix dll inclusion issue`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/startapp_windows.cpp`

## What changed from what, why, and how
The commit corrected behavior in `app/startapp_windows.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(startapp-windows): fix dll inclusion issue`.

Before this commit, the repository reflected the state immediately preceding `f671df1`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/startapp_windows.cpp b/app/startapp_windows.cpp
--- a/app/startapp_windows.cpp
+++ b/app/startapp_windows.cpp
@@ -6,6 +6,14 @@
 #include <vector>
 
 
+std::wstring utf8ToUtf16(const std::string& str) {
+    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
+    std::wstring wstr(size_needed, 0);
+    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size_needed);
+    return wstr;
+}
+
+
 bool isCudaAvailable() {
     for (int i = 9; i <= 20; ++i) {
         std::string dllName = "cudart64_" + std::to_string(i) + ".dll";
@@ -65,7 +73,7 @@ void addToPath(const std::string& directory) {
 
 
 void launchMainApp() {
-    std::string exePath = "bin\\AI File Sorter.exe";
+    std::string exePath = "AI File Sorter.exe";
     if (WinExec(exePath.c_str(), SW_SHOW) < 32) {
         std::cerr << "Failed to launch the application." << std::endl;
     }
```

The excerpt is taken from the commit diff for `fix(startapp-windows): fix dll inclusion issue`. The most relevant surfaces are `app/startapp_windows.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
