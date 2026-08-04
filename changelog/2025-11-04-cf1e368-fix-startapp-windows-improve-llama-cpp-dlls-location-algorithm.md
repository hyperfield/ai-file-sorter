# 2025-11-04: fix(startapp-windows): improve llama.cpp DLLs location algorithm

## Covered commits
- `cf1e368` `2025-11-04` `fix(startapp-windows): improve llama.cpp DLLs location algorithm`

## Motivation
This dependency-management commit kept bundled third-party code in sync with the capabilities or fixes the project needed. Those updates are usually required to unblock platform fixes, tests, or packaging changes in adjacent commits.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/startapp_windows.cpp`

## What changed from what, why, and how
The commit updated dependency pointers or related build references in `app/startapp_windows.cpp`. It moved the repository from older third-party revisions to newer ones needed by the surrounding feature or fix work.

Before this commit, the repository reflected the state immediately preceding `cf1e368`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/startapp_windows.cpp b/app/startapp_windows.cpp
--- a/app/startapp_windows.cpp
+++ b/app/startapp_windows.cpp
@@ -19,6 +19,41 @@
 
 namespace {
 
+bool enableSecureDllSearch()
+{
+#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0602
+    return SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS) != 0;
+#else
+    // Only available on Windows 7+ with KB2533623. Try to enable if present.
+    typedef BOOL (WINAPI *SetDefaultDllDirectoriesFunc)(DWORD);
+    if (const HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll")) {
+        if (const auto fn = reinterpret_cast<SetDefaultDllDirectoriesFunc>(
+                GetProcAddress(kernel32, "SetDefaultDllDirectories"))) {
+            return fn(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS) != 0;
+        }
+    }
+    return false;
+#endif
+}
+
+void addDllDirectoryChecked(const QString& directory)
+{
+    if (directory.isEmpty()) {
+        return;
+    }
+    const std::wstring wideDir = QDir::toNativeSeparators(directory).toStdWString();
+    if (AddDllDirectory(wideDir.c_str()) == nullptr) {
+        qWarning().noquote()
+            << "AddDllDirectory failed for"
+            << QDir::toNativeSeparators(directory)
+            << "- error" << GetLastError();
+    } else {
+        qInfo().noquote()
+            << "Registered DLL directory"
+            << QDir::toNativeSeparators(directory);
+    }
+}
+
 bool tryLoadLibrary(const QString& name) {
     QLibrary lib(name);
     const bool loaded = lib.load();
@@ -68,15 +103,6 @@ void appendToProcessPath(const QString& directory) {
     qInfo().noquote() << "Current PATH:" << QString::fromUtf8(qgetenv("PATH"));
 }
 
-void addDllSearchDirectory(const QString& directory) {
-    if (directory.isEmpty()) {
-        return;
-    }
-
-    const std::wstring wideDir = QDir::toNativeSeparators(directory).toStdWString();
-    AddDllDirectory(wideDir.c_str());
-}
-
 bool promptCudaDownload() {
     const auto response = QMessageBox::warning(
         nullptr,
```

The excerpt is taken from the commit diff for `fix(startapp-windows): improve llama.cpp DLLs location algorithm`. The most relevant surfaces are `app/startapp_windows.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
