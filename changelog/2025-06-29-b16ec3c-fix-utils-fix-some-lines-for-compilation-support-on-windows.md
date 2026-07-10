# 2025-06-29: fix(utils): fix some lines for compilation support on Windows

## Covered commits
- `b16ec3c` `2025-06-29` `fix(utils): fix some lines for compilation support on Windows`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/Utils.cpp`

## What changed from what, why, and how
The commit corrected behavior in `app/lib/Utils.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(utils): fix some lines for compilation support on Windows`.

Before this commit, the repository reflected the state immediately preceding `b16ec3c`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/Utils.cpp b/app/lib/Utils.cpp
--- a/app/lib/Utils.cpp
+++ b/app/lib/Utils.cpp
@@ -184,32 +184,41 @@ int Utils::get_ngl(int vram_mb) {
 
 
 int Utils::determine_ngl_cuda() {
-    void* lib = dlopen("libcudart.so", RTLD_LAZY);
+#ifdef _WIN32
+    std::string dllName = get_cudart_dll_name();  // returns std::string
+    LibraryHandle lib = loadLibrary(dllName.c_str());
+#else
+    const char* dllName = "libcudart.so";
+    LibraryHandle lib = loadLibrary(dllName);
+#endif
+
     if (!lib) {
-        std::cerr << "Failed to load libcudart.so\n";
+        std::cerr << "Failed to load CUDA runtime library: " << dllName << "\n";
         return 0;
     }
 
     using cudaGetDeviceProperties_t = int (*)(void*, int);
     using cudaMemGetInfo_t = int (*)(size_t*, size_t*);
 
-    auto cudaGetDeviceProperties = (cudaGetDeviceProperties_t)dlsym(lib, "cudaGetDeviceProperties");
-    auto cudaMemGetInfo = (cudaMemGetInfo_t)dlsym(lib, "cudaMemGetInfo");
+    auto cudaGetDeviceProperties = reinterpret_cast<cudaGetDeviceProperties_t>(
+        getSymbol(lib, "cudaGetDeviceProperties"));
+    auto cudaMemGetInfo = reinterpret_cast<cudaMemGetInfo_t>(
+        getSymbol(lib, "cudaMemGetInfo"));
 
     if (!cudaGetDeviceProperties) {
-        std::cerr << "Failed to load cudaGetDeviceProperties\n";
-        dlclose(lib);
+        std::cerr << "Failed to load symbol: cudaGetDeviceProperties\n";
+        closeLibrary(lib);
         return 0;
     }
 
-    constexpr size_t cudaDevicePropSize = 2560;  // large enough for any CUDA version <= 12.x
+    constexpr size_t cudaDevicePropSize = 2560;
     alignas(std::max_align_t) uint8_t prop_buffer[cudaDevicePropSize];
     std::memset(prop_buffer, 0, sizeof(prop_buffer));
 
     int device = 0;
     if (cudaGetDeviceProperties(prop_buffer, device) != 0) {
         std::cerr << "Warning: cudaGetDeviceProperties failed\n";
-        dlclose(lib);
+        closeLibrary(lib);
         return 0;
     }
 
@@ -217,10 +226,12 @@ int Utils::determine_ngl_cuda() {
 
     if (cudaMemGetInfo) {
         size_t free_bytes = 0;
-        cudaMemGetInfo(&free_bytes, &total_mem_bytes);  // override total memory with accurate value
+        if (cudaMemGetInfo(&free_bytes, &total_mem_bytes) != 0) {
+            std::cerr << "Warning: cudaMemGetInfo failed\n";
+        }
     }
 
-    dlclose(lib);
+    closeLibrary(lib);
 
     int vram_mb = static_cast<int>(total_mem_bytes / (1024 * 1024));
     return get_ngl(vram_mb);
```

The excerpt is taken from the commit diff for `fix(utils): fix some lines for compilation support on Windows`. The most relevant surfaces are `app/lib/Utils.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
