# 2025-07-10: chore(readme): update to reflect changes

## Covered commits
- `fd018e8` `2025-07-10` `chore(readme): update to reflect changes`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit updated documentation artifacts touching `README.md`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `fd018e8`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -62,7 +62,7 @@ AI File Sorter is a powerful, cross-platform desktop application that automates
 
 File categorization with local LLMs is completely free of charge.
 
-If you want file categorization with ChatGPT, you will need to get an OpenAI API key and add a minimal balance to it for this program to work. Categorization is quite cheap, so $0.01 will be enough to categorize a relatively large number of files. The instructions on how to integrate your API key into the app are given below.
+If you want file categorization with ChatGPT, you will need to get an OpenAI API key and add a minimal balance to it for this program to work. Categorization is quite cheap, so $0.01 will be enough to categorize a relatively large number of files. The instructions on how to integrate your API key into the app are given below. You can also download a [Release](https://github.com/hyperfield/ai-file-sorter/releases) version, which has an embedded API key.
 
 ### Windows
 
@@ -111,14 +111,18 @@ pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-gtk3 mingw-w64-x86_64-gtkm
     -DCMAKE_CXX_COMPILER="C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe"
     ```
 
-    **Important**: If you don't have an Nvidia GPU or if you didn't install the CUDA Toolkit in step 5, change `-DGGML_CUDA=ON` to `-DGGML_CUDA=OFF` in this script.
-
     Save any changes.
 
 8. In `Developer PowerShell for VS 2022`, run
 
+    **If you have CUDA**:
+    ```
+    powershell -ExecutionPolicy Bypass -File .\build_llama_windows.ps1 cuda=on
+    ```
+
+    **If you don't have CUDA**:
     ```
-    powershell -ExecutionPolicy Bypass -File .\build_llama_windows.ps1
+    powershell -ExecutionPolicy Bypass -File .\build_llama_windows.ps1 cuda=off
     ```
 
 9. **Optional** (not needed if you want to use only local LLMs for file sorting). Go to [API Key, Obfuscation, and Encryption](#api-key-obfuscation-and-encryption) and complete all steps there before proceeding to step 6 here. The app won't work otherwise.
```

The excerpt is taken from the commit diff for `chore(readme): update to reflect changes`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
