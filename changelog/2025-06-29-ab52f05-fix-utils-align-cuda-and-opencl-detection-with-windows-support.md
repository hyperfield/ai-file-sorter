# 2025-06-29: fix(utils): align CUDA and OpenCL detection with Windows support

## Covered commits
- `ab52f05` `2025-06-29` `fix(utils): align CUDA and OpenCL detection with Windows support`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/Utils.hpp`
- `M` `app/lib/Utils.cpp`

## What changed from what, why, and how
The commit corrected behavior in `app/include/Utils.hpp`, `app/lib/Utils.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(utils): align CUDA and OpenCL detection with Windows support`.

Before this commit, the repository reflected the state immediately preceding `ab52f05`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/Utils.hpp b/app/include/Utils.hpp
--- a/app/include/Utils.hpp
+++ b/app/include/Utils.hpp
@@ -28,6 +28,8 @@ public:
     static std::string make_default_path_to_file_from_download_url(std::string url);
     static bool is_cuda_available();
     static bool is_opencl_available(std::vector<std::string>* device_names = nullptr);
+    static int get_installed_cuda_runtime_version();
+    static std::string get_cudart_dll_name();
 
 private:
     static int get_ngl(int vram_mb);
diff --git a/app/lib/Utils.cpp b/app/lib/Utils.cpp
index 06650bd..addd31f 100644
--- a/app/lib/Utils.cpp
+++ b/app/lib/Utils.cpp
@@ -1,6 +1,5 @@
 #include "Utils.hpp"
 #include <cstring>  // for memset
-#include <dlfcn.h>
 #include <filesystem>
 #include <stdlib.h>
 #include <string>
```

The excerpt is taken from the commit diff for `fix(utils): align CUDA and OpenCL detection with Windows support`. The most relevant surfaces are `app/include/Utils.hpp`, `app/lib/Utils.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
