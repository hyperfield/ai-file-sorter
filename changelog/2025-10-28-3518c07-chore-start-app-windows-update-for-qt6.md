# 2025-10-28: chore(start-app-windows): update for Qt6

## Covered commits
- `3518c07` `2025-10-28` `chore(start-app-windows): update for Qt6`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/startapp_windows.cpp`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/startapp_windows.cpp`. It changed the repository support state, metadata, or supporting files in the way described by `chore(start-app-windows): update for Qt6`.

Before this commit, the repository reflected the state immediately preceding `3518c07`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/startapp_windows.cpp b/app/startapp_windows.cpp
--- a/app/startapp_windows.cpp
+++ b/app/startapp_windows.cpp
@@ -1,147 +1,163 @@
-#include <cstdio>
-#include <iostream>
-#include <cstdlib>
-#include <shlobj.h>
-#include <string>
-#include <windows.h>
-#include <vector>
-
+#include <QApplication>
+#include <QCoreApplication>
+#include <QDir>
+#include <QFileInfo>
+#include <QDebug>
+#include <QMessageBox>
+#include <QProcess>
+#include <QProcessEnvironment>
+#include <QLibrary>
+#include <QDesktopServices>
+#include <QUrl>
+#include <QByteArray>
+#include <QObject>
+#include <QStringList>
 
+#include <cstdlib>
 
-typedef unsigned int cl_uint;
-typedef int cl_int;
-typedef void* cl_platform_id;
-typedef void* cl_device_id;
-
-#define CL_SUCCESS 0
-#define CL_DEVICE_TYPE_ALL 0xFFFFFFFF
+#include <windows.h>
 
+namespace {
 
-std::wstring utf8ToUtf16(const std::string& str) {
-    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
-    std::wstring wstr(size_needed, 0);
-    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size_needed);
-    return wstr;
+bool tryLoadLibrary(const QString& name) {
+    QLibrary lib(name);
+    const bool loaded = lib.load();
+    if (loaded) {
+        lib.unload();
+    }
+    return loaded;
 }
 
-
 bool isCudaAvailable() {
-    for (int i = 9; i <= 20; ++i) {
-        std::string dllName = "cudart64_" + std::to_string(i) + ".dll";
-        HMODULE lib = LoadLibraryA(dllName.c_str());
-        if (lib) {
-            FreeLibrary(lib);
+    for (int version = 9; version <= 20; ++version) {
+        const QString runtime = QStringLiteral("cudart64_%1").arg(version);
+        if (tryLoadLibrary(runtime)) {
             return true;
         }
     }
     return false;
 }
 
-
 bool isNvidiaDriverAvailable() {
-    const char* dlls[] = {
-        "nvml.dll",
-        "nvcuda.dll",
-        "nvapi64.dll"
+    static const QStringList driverCandidates = {
+        QStringLiteral("nvml"),
+        QStringLiteral("nvcuda"),
+        QStringLiteral("nvapi64")
     };
 
-    for (const auto& dll : dlls) {
-        HMODULE lib = LoadLibraryA(dll);
-        if (lib) {
-            FreeLibrary(lib);
+    for (const QString& dll : driverCandidates) {
+        if (tryLoadLibrary(dll)) {
             return true;
         }
     }
     return false;
 }
 
+void appendToProcessPath(const QString& directory) {
+    if (directory.isEmpty()) {
+        return;
+    }
 
-std::string getExecutableDirectory() {
```

The excerpt is taken from the commit diff for `chore(start-app-windows): update for Qt6`. The most relevant surfaces are `app/startapp_windows.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
