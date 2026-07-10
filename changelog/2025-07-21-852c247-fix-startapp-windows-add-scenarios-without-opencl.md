# 2025-07-21: fix(startapp-windows): add scenarios without opencl

## Covered commits
- `852c247` `2025-07-21` `fix(startapp-windows): add scenarios without opencl`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/startapp_windows.cpp`

## What changed from what, why, and how
The commit corrected behavior in `app/startapp_windows.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(startapp-windows): add scenarios without opencl`.

Before this commit, the repository reflected the state immediately preceding `852c247`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/startapp_windows.cpp b/app/startapp_windows.cpp
--- a/app/startapp_windows.cpp
+++ b/app/startapp_windows.cpp
@@ -15,6 +15,16 @@ bool isCudaAvailable() {
 }
 
 
+bool isOpenCLAvailable() {
+    HMODULE opencl = LoadLibraryA("OpenCL.dll");
+    if (opencl) {
+        FreeLibrary(opencl);
+        return true;
+    }
+    return false;
+}
+
+
 std::string getExecutableDirectory() {
     WCHAR buffer[MAX_PATH];
     GetModuleFileNameW(NULL, buffer, MAX_PATH);
@@ -68,13 +78,17 @@ int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
         return EXIT_FAILURE;
     }
 
-    std::string dllPath = exeDir + "\\lib";
-    addToPath(dllPath);
+    addToPath(exeDir + "\\lib");
+
+    bool hasCuda = isCudaAvailable();
+    bool hasOpenCL = isOpenCLAvailable();
+
+    std::string folderName;
+    folderName += hasCuda ? "wcuda" : "wocuda";
+    folderName += hasOpenCL ? "wopencl" : "woopencl";
 
-    bool useCuda = isCudaAvailable();
-    std::string cudaPath = exeDir + "\\lib\\ggml\\wcuda";
-    std::string noCudaPath = exeDir + "\\lib\\ggml\\wocuda";
-    addToPath(useCuda ? cudaPath : noCudaPath);
+    std::string ggmlPath = exeDir + "\\lib\\ggml\\" + folderName;
+    addToPath(ggmlPath);
 
     launchMainApp();
     return EXIT_SUCCESS;
```

The excerpt is taken from the commit diff for `fix(startapp-windows): add scenarios without opencl`. The most relevant surfaces are `app/startapp_windows.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
