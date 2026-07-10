# 2025-07-21: chore(localllmclient): prevent cuda and opencl detection for macOS

## Covered commits
- `9d33edb` `2025-07-21` `chore(localllmclient): prevent cuda and opencl detection for macOS`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/LocalLLMClient.cpp`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/lib/LocalLLMClient.cpp`. It changed the repository support state, metadata, or supporting files in the way described by `chore(localllmclient): prevent cuda and opencl detection for macOS`.

Before this commit, the repository reflected the state immediately preceding `9d33edb`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/LocalLLMClient.cpp b/app/lib/LocalLLMClient.cpp
--- a/app/lib/LocalLLMClient.cpp
+++ b/app/lib/LocalLLMClient.cpp
@@ -27,22 +27,25 @@ LocalLLMClient::LocalLLMClient(const std::string& model_path)
 
     llama_model_params model_params = llama_model_default_params();
 
-    // int ngl;
-    if (Utils::is_cuda_available()) {
-        model_params.n_gpu_layers = Utils::determine_ngl_cuda();
-        std::cout << "ngl: " << model_params.n_gpu_layers << std::endl;
-    } else {
+    #ifdef GGML_USE_METAL
         model_params.n_gpu_layers = 0;
-        printf("model_params.n_gpu_layers: %d\n", model_params.n_gpu_layers);
-        std::vector<std::string> devices;
-        if (Utils::is_opencl_available(&devices)) {
-            std::cout << "OpenCL is available.\n";
-            for (const auto& dev : devices)
-                std::cout << "Device: " << dev << "\n";
+    #else
+        if (Utils::is_cuda_available()) {
+            model_params.n_gpu_layers = Utils::determine_ngl_cuda();
+            std::cout << "ngl: " << model_params.n_gpu_layers << std::endl;
         } else {
-            std::cout << "OpenCL not found.\n";
+            model_params.n_gpu_layers = 0;
+            printf("model_params.n_gpu_layers: %d\n", model_params.n_gpu_layers);
+            std::vector<std::string> devices;
+            if (Utils::is_opencl_available(&devices)) {
+                std::cout << "OpenCL is available.\n";
+                for (const auto& dev : devices)
+                    std::cout << "Device: " << dev << "\n";
+            } else {
+                std::cout << "OpenCL not found.\n";
+            }
         }
-    }
+    #endif
 
     model = llama_model_load_from_file(model_path.c_str(), model_params);
     if (!model) {
```

The excerpt is taken from the commit diff for `chore(localllmclient): prevent cuda and opencl detection for macOS`. The most relevant surfaces are `app/lib/LocalLLMClient.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
