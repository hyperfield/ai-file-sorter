# 2025-07-22: fix(localllmclient): compute ngl based on available cuda device memory rather than total memory

## Covered commits
- `008daa4` `2025-07-22` `fix(localllmclient): compute ngl based on available cuda device memory rather than total memory`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/LocalLLMClient.cpp`

## What changed from what, why, and how
The commit corrected behavior in `app/lib/LocalLLMClient.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(localllmclient): compute ngl based on available cuda device memory rather than total memory`.

Before this commit, the repository reflected the state immediately preceding `008daa4`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/LocalLLMClient.cpp b/app/lib/LocalLLMClient.cpp
--- a/app/lib/LocalLLMClient.cpp
+++ b/app/lib/LocalLLMClient.cpp
@@ -35,7 +35,8 @@ LocalLLMClient::LocalLLMClient(const std::string& model_path)
             std::cout << "ngl: " << model_params.n_gpu_layers << std::endl;
         } else {
             model_params.n_gpu_layers = 0;
-            printf("model_params.n_gpu_layers: %d\n", model_params.n_gpu_layers);
+            printf("model_params.n_gpu_layers: %d\n",
+                model_params.n_gpu_layers);
             std::vector<std::string> devices;
             if (Utils::is_opencl_available(&devices)) {
                 std::cout << "OpenCL is available.\n";
@@ -71,7 +72,8 @@ LocalLLMClient::LocalLLMClient(const std::string& model_path)
 }
 
 
-std::string LocalLLMClient::make_prompt(const std::string& file_name, FileType file_type)
+std::string LocalLLMClient::make_prompt(const std::string& file_name,
+                                        FileType file_type)
 {
     std::string prompt;
     if (file_type == FileType::File) {
```

The excerpt is taken from the commit diff for `fix(localllmclient): compute ngl based on available cuda device memory rather than total memory`. The most relevant surfaces are `app/lib/LocalLLMClient.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
