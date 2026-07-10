# 2025-07-19: chore(llmselectiondialog): update arrangment and description of LLM for better accuracy

## Covered commits
- `ddb3e63` `2025-07-19` `chore(llmselectiondialog): update arrangment and description of LLM for better accuracy`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/LLMSelectionDialog.cpp`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/lib/LLMSelectionDialog.cpp`. It changed the repository support state, metadata, or supporting files in the way described by `chore(llmselectiondialog): update arrangment and description of LLM for better accuracy`.

Before this commit, the repository reflected the state immediately preceding `ddb3e63`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/LLMSelectionDialog.cpp b/app/lib/LLMSelectionDialog.cpp
--- a/app/lib/LLMSelectionDialog.cpp
+++ b/app/lib/LLMSelectionDialog.cpp
@@ -120,7 +120,7 @@ LLMSelectionDialog::LLMSelectionDialog(Settings& settings) :
     // Descriptions
     GtkWidget *radio_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
     GtkWidget* label_remote_desc = gtk_label_new(
-    "Online LLM – fast, accurate, and always up-to-date. Requires internet connection.");
+    "Fast and accurate, but requires internet connection.");
     gtk_label_set_xalign(GTK_LABEL(label_remote_desc), 0.0f);
     gtk_widget_set_margin_bottom(label_remote_desc, 10);
     GtkWidget* label_3b_desc = gtk_label_new(
@@ -130,7 +130,7 @@ LLMSelectionDialog::LLMSelectionDialog(Settings& settings) :
 
     GtkWidget* label_7b_desc = gtk_label_new(
         "Quite precise. Slower on CPU, but performs much better with GPU acceleration.\n"
-        "Supports: Nvidia (cuBLAS), AMD (ROCm/HIP), Apple Silicon (Metal).");
+        "Supports: Nvidia (CUDA), OpenCL, Apple Silicon (Metal), CPU.");
     gtk_label_set_xalign(GTK_LABEL(label_7b_desc), 0.0f);
     gtk_widget_set_margin_bottom(label_7b_desc, 10);
```

The excerpt is taken from the commit diff for `chore(llmselectiondialog): update arrangment and description of LLM for better accuracy`. The most relevant surfaces are `app/lib/LLMSelectionDialog.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
