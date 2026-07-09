# 2025-10-23: chore(llm-selection-dialog): change the order of LLMs

## Covered commits
- `92fdf5e` `2025-10-23` `chore(llm-selection-dialog): change the order of LLMs`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/external/llama.cpp`
- `M` `app/lib/LLMSelectionDialog.cpp`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/include/external/llama.cpp`, `app/lib/LLMSelectionDialog.cpp`. It changed the repository support state, metadata, or supporting files in the way described by `chore(llm-selection-dialog): change the order of LLMs`.

Before this commit, the repository reflected the state immediately preceding `92fdf5e`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/external/llama.cpp b/app/include/external/llama.cpp
--- a/app/include/external/llama.cpp
+++ b/app/include/external/llama.cpp
@@ -1 +1 @@
-Subproject commit ee09828cb057460b369576410601a3a09279e23c
+Subproject commit 7675c555a13c9f473249e59a54db35032ce8e0fc
diff --git a/app/lib/LLMSelectionDialog.cpp b/app/lib/LLMSelectionDialog.cpp
index 7b60126..e398986 100644
--- a/app/lib/LLMSelectionDialog.cpp
+++ b/app/lib/LLMSelectionDialog.cpp
@@ -81,10 +81,6 @@ void LLMSelectionDialog::setup_ui()
     auto* radio_layout = new QVBoxLayout(radio_container);
     radio_layout->setSpacing(10);
 
-    remote_radio = new QRadioButton(tr("Remote LLM (ChatGPT 4o-mini)"), radio_container);
-    auto* remote_desc = new QLabel(tr("Fast and accurate, but requires internet connection."), radio_container);
-    remote_desc->setWordWrap(true);
-
     local7_radio = new QRadioButton(tr("Local LLM (Mistral 7b Instruct v0.2 Q5)"), radio_container);
     auto* local7_desc = new QLabel(tr("Quite precise. Slower on CPU, but performs much better with GPU acceleration.\nSupports: Nvidia (CUDA), OpenCL, Apple (Metal), CPU."), radio_container);
     local7_desc->setWordWrap(true);
```

The excerpt is taken from the commit diff for `chore(llm-selection-dialog): change the order of LLMs`. The most relevant surfaces are `app/include/external/llama.cpp`, `app/lib/LLMSelectionDialog.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
