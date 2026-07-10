# 2026-01-10: chore(console-output): adapt to development mode

## Covered commits
- `73fead9` `2026-01-10` `chore(console-output): adapt to development mode`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/LlavaImageAnalyzer.hpp`
- `M` `app/lib/CategorizationDialog.cpp`
- `M` `app/lib/LlavaImageAnalyzer.cpp`
- `M` `app/lib/MainApp.cpp`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/include/LlavaImageAnalyzer.hpp`, `app/lib/CategorizationDialog.cpp`, `app/lib/LlavaImageAnalyzer.cpp`, `app/lib/MainApp.cpp`. It changed the repository support state, metadata, or supporting files in the way described by `chore(console-output): adapt to development mode`.

Before this commit, the repository reflected the state immediately preceding `73fead9`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/LlavaImageAnalyzer.hpp b/app/include/LlavaImageAnalyzer.hpp
--- a/app/include/LlavaImageAnalyzer.hpp
+++ b/app/include/LlavaImageAnalyzer.hpp
@@ -29,6 +29,7 @@ public:
         int32_t n_threads = 0;
         float temperature = 0.2f;
         bool use_gpu = true;
+        bool log_visual_output = false;
         std::function<void(int32_t current_batch, int32_t total_batches)> batch_progress;
     };
 
diff --git a/app/lib/CategorizationDialog.cpp b/app/lib/CategorizationDialog.cpp
index dbe8d3b..cacbe59 100644
--- a/app/lib/CategorizationDialog.cpp
+++ b/app/lib/CategorizationDialog.cpp
@@ -698,6 +698,7 @@ void CategorizationDialog::populate_model()
         select_item->setCheckable(true);
         select_item->setCheckState(Qt::Checked);
         select_item->setEditable(false);
+        select_item->setData(Qt::AlignCenter, Qt::TextAlignmentRole);
 
         auto* file_item = new QStandardItem(QString::fromStdString(file.file_name));
         file_item->setEditable(false);
```

The excerpt is taken from the commit diff for `chore(console-output): adapt to development mode`. The most relevant surfaces are `app/include/LlavaImageAnalyzer.hpp`, `app/lib/CategorizationDialog.cpp`, `app/lib/LlavaImageAnalyzer.cpp`, `app/lib/MainApp.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
