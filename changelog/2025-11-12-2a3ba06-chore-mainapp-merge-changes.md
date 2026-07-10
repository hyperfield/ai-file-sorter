# 2025-11-12: chore(mainapp): merge changes

## Covered commits
- `2a3ba06` `2025-11-12` `chore(mainapp): merge changes`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/external/llama.cpp`
- `M` `app/lib/MainApp.cpp`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/include/external/llama.cpp`, `app/lib/MainApp.cpp`. It changed the repository support state, metadata, or supporting files in the way described by `chore(mainapp): merge changes`.

Before this commit, the repository reflected the state immediately preceding `2a3ba06`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/external/llama.cpp b/app/include/external/llama.cpp
--- a/app/include/external/llama.cpp
+++ b/app/include/external/llama.cpp
@@ -1 +1 @@
-Subproject commit 7675c555a13c9f473249e59a54db35032ce8e0fc
+Subproject commit ee09828cb057460b369576410601a3a09279e23c
diff --git a/app/lib/MainApp.cpp b/app/lib/MainApp.cpp
index 6934c39..3bfc032 100644
--- a/app/lib/MainApp.cpp
+++ b/app/lib/MainApp.cpp
@@ -204,6 +204,7 @@ void MainApp::setup_file_explorer()
         file_explorer_view->scrollTo(home_index);
     }
     file_explorer_view->setHeaderHidden(false);
+    file_explorer_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
     file_explorer_view->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
     file_explorer_view->setColumnHidden(1, true);
     file_explorer_view->setColumnHidden(2, true);
```

The excerpt is taken from the commit diff for `chore(mainapp): merge changes`. The most relevant surfaces are `app/include/external/llama.cpp`, `app/lib/MainApp.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
