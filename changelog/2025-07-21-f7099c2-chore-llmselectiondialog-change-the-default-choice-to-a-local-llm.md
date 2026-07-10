# 2025-07-21: chore(llmselectiondialog): change the default choice to a local llm

## Covered commits
- `f7099c2` `2025-07-21` `chore(llmselectiondialog): change the default choice to a local llm`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/LLMSelectionDialog.cpp`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/lib/LLMSelectionDialog.cpp`. It changed the repository support state, metadata, or supporting files in the way described by `chore(llmselectiondialog): change the default choice to a local llm`.

Before this commit, the repository reflected the state immediately preceding `f7099c2`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/LLMSelectionDialog.cpp b/app/lib/LLMSelectionDialog.cpp
--- a/app/lib/LLMSelectionDialog.cpp
+++ b/app/lib/LLMSelectionDialog.cpp
@@ -124,13 +124,13 @@ LLMSelectionDialog::LLMSelectionDialog(Settings& settings) :
     gtk_label_set_xalign(GTK_LABEL(label_remote_desc), 0.0f);
     gtk_widget_set_margin_bottom(label_remote_desc, 10);
     GtkWidget* label_3b_desc = gtk_label_new(
-    "Not very precise, but works quickly even on CPUs. Good for lightweight local use.");
+    "Less precise, but works quickly even on CPUs. Good for lightweight local use.");
     gtk_label_set_xalign(GTK_LABEL(label_3b_desc), 0.0f);
     gtk_widget_set_margin_bottom(label_3b_desc, 10);
 
     GtkWidget* label_7b_desc = gtk_label_new(
         "Quite precise. Slower on CPU, but performs much better with GPU acceleration.\n"
-        "Supports: Nvidia (CUDA), OpenCL, Apple Silicon (Metal), CPU.");
+        "Supports: Nvidia (CUDA), OpenCL, Apple (Metal), CPU.");
     gtk_label_set_xalign(GTK_LABEL(label_7b_desc), 0.0f);
     gtk_widget_set_margin_bottom(label_7b_desc, 10);
 
@@ -285,6 +285,8 @@ LLMSelectionDialog::LLMSelectionDialog(Settings& settings) :
             selected_choice = LLMChoice::Local_7b;
             break;
         default:
+            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(local_llm_7b_button), TRUE);
+            selected_choice = LLMChoice::Local_7b;
             break;
     }
```

The excerpt is taken from the commit diff for `chore(llmselectiondialog): change the default choice to a local llm`. The most relevant surfaces are `app/lib/LLMSelectionDialog.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
