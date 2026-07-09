# 2025-07-22: fix(utils): compute ngl based on available cuda device memory rather than total memory

## Covered commits
- `c8fd11e` `2025-07-22` `fix(utils): compute ngl based on available cuda device memory rather than total memory`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/Utils.cpp`

## What changed from what, why, and how
The commit corrected behavior in `app/lib/Utils.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(utils): compute ngl based on available cuda device memory rather than total memory`.

Before this commit, the repository reflected the state immediately preceding `c8fd11e`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/Utils.cpp b/app/lib/Utils.cpp
--- a/app/lib/Utils.cpp
+++ b/app/lib/Utils.cpp
@@ -225,17 +225,20 @@ int Utils::determine_ngl_cuda() {
     }
 
     size_t total_mem_bytes = *reinterpret_cast<size_t*>(prop_buffer);
+    size_t free_bytes = 0;
 
     if (cudaMemGetInfo) {
-        size_t free_bytes = 0;
         if (cudaMemGetInfo(&free_bytes, &total_mem_bytes) != 0) {
             std::cerr << "Warning: cudaMemGetInfo failed\n";
+            free_bytes = 0;
         }
     }
 
     closeLibrary(lib);
 
-    int vram_mb = static_cast<int>(total_mem_bytes / (1024 * 1024));
+    size_t usable_bytes = free_bytes > 0 ? free_bytes : total_mem_bytes;
+    int vram_mb = static_cast<int>(usable_bytes / (1024 * 1024));
+
     return get_ngl(vram_mb);
 }
```

The excerpt is taken from the commit diff for `fix(utils): compute ngl based on available cuda device memory rather than total memory`. The most relevant surfaces are `app/lib/Utils.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
