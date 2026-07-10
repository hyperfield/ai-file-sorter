# 2025-07-22: fix(local-llm): optimization and stability improvement for local LLMs

## Covered commits
- `20f1368` `2025-07-22` `fix(local-llm): optimization and stability improvement for local LLMs`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/CategorizationProgressDialog.cpp`
- `M` `app/lib/LocalLLMClient.cpp`
- `M` `app/lib/MainApp.cpp`
- `M` `app/lib/Utils.cpp`

## What changed from what, why, and how
The commit corrected behavior in `app/lib/CategorizationProgressDialog.cpp`, `app/lib/LocalLLMClient.cpp`, `app/lib/MainApp.cpp`, `app/lib/Utils.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(local-llm): optimization and stability improvement for local LLMs`.

Before this commit, the repository reflected the state immediately preceding `20f1368`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/CategorizationProgressDialog.cpp b/app/lib/CategorizationProgressDialog.cpp
--- a/app/lib/CategorizationProgressDialog.cpp
+++ b/app/lib/CategorizationProgressDialog.cpp
@@ -124,8 +124,7 @@ void CategorizationProgressDialog::hide()
 }
 
 
-CategorizationProgressDialog::~CategorizationProgressDialog()
-{
+CategorizationProgressDialog::~CategorizationProgressDialog() {
     if (m_Dialog) {
         gtk_widget_destroy(m_Dialog);
         m_Dialog = nullptr;
@@ -138,4 +137,4 @@ CategorizationProgressDialog::~CategorizationProgressDialog()
 
     m_StopButton = nullptr;
     buffer = nullptr;
-}
\ No newline at end of file
+}
```

The excerpt is taken from the commit diff for `fix(local-llm): optimization and stability improvement for local LLMs`. The most relevant surfaces are `app/lib/CategorizationProgressDialog.cpp`, `app/lib/LocalLLMClient.cpp`, `app/lib/MainApp.cpp`, `app/lib/Utils.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
