# 2026-03-21: chore(documents):  update

## Covered commits
- `d66ce7b` `2026-03-21` `chore(documents):  update`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `README.md`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `README.md`. It changed the repository support state, metadata, or supporting files in the way described by `chore(documents):  update`.

Before this commit, the repository reflected the state immediately preceding `d66ce7b`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/README.md b/README.md
--- a/README.md
+++ b/README.md
@@ -36,14 +36,14 @@ Instead of relying on fixed rules, the app gradually builds an internal understa
 Categories (and optional subcategories) are suggested for each file, and for supported file types, rename suggestions are provided as well. Once you confirm, the required folders are created automatically and files are sorted accordingly.
 
 Privacy-first by design:
-AI File Sorter runs entirely on your device, using local AI models such as LLaMa 3B (Q4) and Mistral 7B. No files, filenames, images, or metadata are uploaded anywhere, and no telemetry is sent. An internet connection is only used if you explicitly choose to enable a remote model.
+AI File Sorter can run entirely on your device, using local AI models such as Llama 3B (Q4) and Mistral 7B. No files, filenames, images, or metadata are uploaded anywhere, and no telemetry is sent. An internet connection is only needed if you explicitly choose to enable a remote model.
 
 ---
 
 #### How It Works
 
 1. Point the app at a folder or drive  
-2. Files (and image content, when applicable) are analyzed locally  
+2. Files (and image content, when applicable) are analyzed using the selected local or remote model  
 3. Category and rename suggestions are generated  
 4. You review and adjust if needed - done  
 
@@ -82,6 +82,7 @@ AI File Sorter runs entirely on your device, using local AI models such as LLaMa
   - [Using your OpenAI API key](#using-your-openai-api-key)
   - [Using your Gemini API key](#using-your-gemini-api-key)
   - [Testing](#testing)
+  - [Diagnostics](#diagnostics)
   - [How to Use](#how-to-use)
   - [Sorting a Remote Directory (e.g., NAS)](#sorting-a-remote-directory-eg-nas)
   - [Contributing](#contributing)
```

The excerpt is taken from the commit diff for `chore(documents):  update`. The most relevant surfaces are `README.md`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
