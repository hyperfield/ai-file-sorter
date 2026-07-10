# 2025-08-01: fix(startapp-windows): remove OpenCL check for stability, add CUDA driver check and Toolkit download dialog

## Covered commits
- `411b2c7` `2025-08-01` `fix(startapp-windows): remove OpenCL check for stability, add CUDA driver check and Toolkit download dialog`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/startapp_windows.cpp`

## What changed from what, why, and how
The commit corrected behavior in `app/startapp_windows.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(startapp-windows): remove OpenCL check for stability, add CUDA driver check and Toolkit download dialog`.

Before this commit, the repository reflected the state immediately preceding `411b2c7`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/startapp_windows.cpp b/app/startapp_windows.cpp
--- a/app/startapp_windows.cpp
+++ b/app/startapp_windows.cpp
@@ -5,6 +5,8 @@
 #include <windows.h>
 #include <vector>
 
+#include <gtk/gtk.h>
+
 
 typedef unsigned int cl_uint;
 typedef int cl_int;
@@ -15,48 +17,6 @@ typedef void* cl_device_id;
 #define CL_DEVICE_TYPE_ALL 0xFFFFFFFF
 
 
-bool isOpenCLUsable() {
-    typedef unsigned int cl_uint;
-    typedef int cl_int;
-    typedef void* cl_platform_id;
-    typedef void* cl_device_id;
-
-    #define CL_SUCCESS 0
-    #define CL_DEVICE_TYPE_ALL 0xFFFFFFFF
-
-    typedef cl_int (__stdcall *clGetPlatformIDs_t)(cl_uint, cl_platform_id*, cl_uint*);
-    typedef cl_int (__stdcall *clGetDeviceIDs_t)(cl_platform_id, cl_uint, cl_uint, cl_device_id*, cl_uint*);
-
-    HMODULE openclLib = LoadLibraryA("OpenCL.dll");
-    if (!openclLib) return false;
-
-    auto clGetPlatformIDs = (clGetPlatformIDs_t)GetProcAddress(openclLib, "clGetPlatformIDs");
-    auto clGetDeviceIDs   = (clGetDeviceIDs_t)GetProcAddress(openclLib, "clGetDeviceIDs");
-
-    if (!clGetPlatformIDs || !clGetDeviceIDs) {
-        FreeLibrary(openclLib);
-        return false;
-    }
-
-    cl_platform_id platform;
-    cl_uint numPlatforms = 0;
-    if (clGetPlatformIDs(1, &platform, &numPlatforms) != CL_SUCCESS || numPlatforms == 0) {
-        FreeLibrary(openclLib);
-        return false;
-    }
-
-    cl_device_id device;
-    cl_uint numDevices = 0;
-    if (clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 1, &device, &numDevices) != CL_SUCCESS || numDevices == 0) {
-        FreeLibrary(openclLib);
-        return false;
-    }
-
-    FreeLibrary(openclLib);
-    return true;
-}
-
-
 std::wstring utf8ToUtf16(const std::string& str) {
     int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
     std::wstring wstr(size_needed, 0);
```

The excerpt is taken from the commit diff for `fix(startapp-windows): remove OpenCL check for stability, add CUDA driver check and Toolkit download dialog`. The most relevant surfaces are `app/startapp_windows.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
