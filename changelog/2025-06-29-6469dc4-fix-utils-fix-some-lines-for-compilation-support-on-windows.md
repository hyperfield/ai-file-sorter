# 2025-06-29: fix(utils): fix some lines for compilation support on Windows

## Covered commits
- `6469dc4` `2025-06-29` `fix(utils): fix some lines for compilation support on Windows`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/Utils.cpp`

## What changed from what, why, and how
The commit corrected behavior in `app/lib/Utils.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(utils): fix some lines for compilation support on Windows`.

Before this commit, the repository reflected the state immediately preceding `6469dc4`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/Utils.cpp b/app/lib/Utils.cpp
--- a/app/lib/Utils.cpp
+++ b/app/lib/Utils.cpp
@@ -258,16 +258,16 @@ std::string Utils::get_default_llm_destination()
     if (Utils::is_os_windows()) {
         const char* appdata = std::getenv("APPDATA");
         if (!appdata) throw std::runtime_error("APPDATA not set");
-        return std::filesystem::path(appdata) / "aifilesorter" / "llms";
+        return (std::filesystem::path(appdata) / "aifilesorter" / "llms").string();
     }
 
     if (!home) throw std::runtime_error("HOME not set");
 
     if (Utils::is_os_macos()) {
-        return std::filesystem::path(home) / "Library" / "Application Support" / "aifilesorter" / "llms";
+        return (std::filesystem::path(home) / "Library" / "Application Support" / "aifilesorter" / "llms").string();
     }
 
-    return std::filesystem::path(home) / ".local" / "share" / "aifilesorter" / "llms";
+    return (std::filesystem::path(home) / ".local" / "share" / "aifilesorter" / "llms").string();
 }
 
 
@@ -329,57 +329,6 @@ bool Utils::is_cuda_available() {
 }
 
 
-bool Utils::is_opencl_available(std::vector<std::string>* device_names)
-{
-#ifdef _WIN32
-    LibraryHandle handle = loadLibrary("OpenCL.dll");
-#else
-    LibraryHandle handle = loadLibrary("libOpenCL.so");
-#endif
-
-    if (!handle) return false;
-
-    using clGetPlatformIDs_t = cl_int (*)(cl_uint, cl_platform_id*, cl_uint*);
-    using clGetDeviceIDs_t = cl_int (*)(cl_platform_id, cl_device_type, cl_uint, cl_device_id*, cl_uint*);
-    using clGetDeviceInfo_t = cl_int (*)(cl_device_id, cl_uint, size_t, void*, size_t*);
-
-    auto clGetPlatformIDs = reinterpret_cast<clGetPlatformIDs_t>(getSymbol(handle, "clGetPlatformIDs"));
-    auto clGetDeviceIDs = reinterpret_cast<clGetDeviceIDs_t>(getSymbol(handle, "clGetDeviceIDs"));
-    auto clGetDeviceInfo = reinterpret_cast<clGetDeviceInfo_t>(getSymbol(handle, "clGetDeviceInfo"));
-
-    if (!clGetPlatformIDs || !clGetDeviceIDs || !clGetDeviceInfo) {
-        closeLibrary(handle);
-        return false;
-    }
-
-    cl_platform_id platform;
-    cl_uint num_platforms = 0;
-    if (clGetPlatformIDs(1, &platform, &num_platforms) != CL_SUCCESS || num_platforms == 0) {
-        closeLibrary(handle);
-        return false;
-    }
-
-    cl_device_id devices[4];
-    cl_uint num_devices = 0;
-    if (clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 4, devices, &num_devices) != CL_SUCCESS || num_devices == 0) {
-        closeLibrary(handle);
-        return false;
-    }
-
-    if (device_names) {
-        for (cl_uint i = 0; i < num_devices; ++i) {
-            char name[256];
-            if (clGetDeviceInfo(devices[i], CL_DEVICE_NAME, sizeof(name), name, nullptr) == CL_SUCCESS) {
-                device_names->emplace_back(name);
-            }
-        }
-    }
-
-    closeLibrary(handle);
-    return true;
-}
-
-
 #ifdef _WIN32
 int Utils::get_installed_cuda_runtime_version() {
     HMODULE hCuda = LoadLibraryA("nvcuda.dll");
```

The excerpt is taken from the commit diff for `fix(utils): fix some lines for compilation support on Windows`. The most relevant surfaces are `app/lib/Utils.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
