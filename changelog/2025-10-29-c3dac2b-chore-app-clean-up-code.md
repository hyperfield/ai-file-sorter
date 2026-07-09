# 2025-10-29: chore(app): clean-up code

## Covered commits
- `c3dac2b` `2025-10-29` `chore(app): clean-up code`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/LLMSelectionDialog.hpp`
- `M` `app/include/external/llama.cpp`
- `M` `app/lib/LLMSelectionDialog.cpp`
- `M` `app/lib/LocalLLMClient.cpp`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/include/LLMSelectionDialog.hpp`, `app/include/external/llama.cpp`, `app/lib/LLMSelectionDialog.cpp`, `app/lib/LocalLLMClient.cpp`. It changed the repository support state, metadata, or supporting files in the way described by `chore(app): clean-up code`.

Before this commit, the repository reflected the state immediately preceding `c3dac2b`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/LLMSelectionDialog.hpp b/app/include/LLMSelectionDialog.hpp
--- a/app/include/LLMSelectionDialog.hpp
+++ b/app/include/LLMSelectionDialog.hpp
@@ -38,7 +38,6 @@ private:
     void set_status_message(const QString& message);
     std::string current_download_env_var() const;
 
-    Settings& settings;
     LLMChoice selected_choice{LLMChoice::Unset};
 
     QRadioButton* remote_radio{nullptr};
diff --git a/app/include/external/llama.cpp b/app/include/external/llama.cpp
index ee09828..7675c55 160000
--- a/app/include/external/llama.cpp
+++ b/app/include/external/llama.cpp
@@ -1 +1 @@
-Subproject commit ee09828cb057460b369576410601a3a09279e23c
+Subproject commit 7675c555a13c9f473249e59a54db35032ce8e0fc
```

The excerpt is taken from the commit diff for `chore(app): clean-up code`. The most relevant surfaces are `app/include/LLMSelectionDialog.hpp`, `app/include/external/llama.cpp`, `app/lib/LLMSelectionDialog.cpp`, `app/lib/LocalLLMClient.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
