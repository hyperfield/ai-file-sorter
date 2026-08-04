# 2025-07-10: fix(localllmclient.cpp): correct cuda and opencl handling

## Covered commits
- `b07f526` `2025-07-10` `fix(localllmclient.cpp): correct cuda and opencl handling`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/LocalLLMClient.cpp`

## What changed from what, why, and how
The commit corrected behavior in `app/lib/LocalLLMClient.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(localllmclient.cpp): correct cuda and opencl handling`.

Before this commit, the repository reflected the state immediately preceding `b07f526`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/LocalLLMClient.cpp b/app/lib/LocalLLMClient.cpp
--- a/app/lib/LocalLLMClient.cpp
+++ b/app/lib/LocalLLMClient.cpp
@@ -32,9 +32,8 @@ LocalLLMClient::LocalLLMClient(const std::string& model_path)
         model_params.n_gpu_layers = Utils::determine_ngl_cuda();
         std::cout << "ngl: " << model_params.n_gpu_layers << std::endl;
     } else {
-        model_params.n_gpu_layers = 18;
+        model_params.n_gpu_layers = 0;
         printf("model_params.n_gpu_layers: %d\n", model_params.n_gpu_layers);
-        // model_params.n_gpu_layers = 0;
         std::vector<std::string> devices;
         if (Utils::is_opencl_available(&devices)) {
             std::cout << "OpenCL is available.\n";
@@ -44,8 +43,6 @@ LocalLLMClient::LocalLLMClient(const std::string& model_path)
             std::cout << "OpenCL not found.\n";
         }
     }
-    // ngl = model_params.n_gpu_layers;
-    // printf("ngl set to %d\n", ngl);
 
     model = llama_model_load_from_file(model_path.c_str(), model_params);
     if (!model) {
```

The excerpt is taken from the commit diff for `fix(localllmclient.cpp): correct cuda and opencl handling`. The most relevant surfaces are `app/lib/LocalLLMClient.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
