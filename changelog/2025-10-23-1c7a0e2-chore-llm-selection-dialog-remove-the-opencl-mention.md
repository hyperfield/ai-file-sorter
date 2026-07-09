# 2025-10-23: chore(llm-selection-dialog): remove the OpenCL mention

## Covered commits
- `1c7a0e2` `2025-10-23` `chore(llm-selection-dialog): remove the OpenCL mention`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/LLMSelectionDialog.cpp`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/lib/LLMSelectionDialog.cpp`. It changed the repository support state, metadata, or supporting files in the way described by `chore(llm-selection-dialog): remove the OpenCL mention`.

Before this commit, the repository reflected the state immediately preceding `1c7a0e2`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/LLMSelectionDialog.cpp b/app/lib/LLMSelectionDialog.cpp
--- a/app/lib/LLMSelectionDialog.cpp
+++ b/app/lib/LLMSelectionDialog.cpp
@@ -82,7 +82,7 @@ void LLMSelectionDialog::setup_ui()
     radio_layout->setSpacing(10);
 
     local7_radio = new QRadioButton(tr("Local LLM (Mistral 7b Instruct v0.2 Q5)"), radio_container);
-    auto* local7_desc = new QLabel(tr("Quite precise. Slower on CPU, but performs much better with GPU acceleration.\nSupports: Nvidia (CUDA), OpenCL, Apple (Metal), CPU."), radio_container);
+    auto* local7_desc = new QLabel(tr("Quite precise. Slower on CPU, but performs much better with GPU acceleration.\nSupports: Nvidia (CUDA), Apple (Metal), CPU."), radio_container);
     local7_desc->setWordWrap(true);
 
     local3_radio = new QRadioButton(tr("Local LLM (LLaMa 3b v3.2 Instruct Q8)"), radio_container);
```

The excerpt is taken from the commit diff for `chore(llm-selection-dialog): remove the OpenCL mention`. The most relevant surfaces are `app/lib/LLMSelectionDialog.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
