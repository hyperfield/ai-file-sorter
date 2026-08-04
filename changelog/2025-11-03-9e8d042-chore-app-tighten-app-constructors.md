# 2025-11-03: chore(app): tighten app constructors

## Covered commits
- `9e8d042` `2025-11-03` `chore(app): tighten app constructors`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/DatabaseManager.hpp`
- `M` `app/include/LLMClient.hpp`
- `M` `app/include/LLMDownloader.hpp`
- `M` `app/include/Updater.hpp`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/include/DatabaseManager.hpp`, `app/include/LLMClient.hpp`, `app/include/LLMDownloader.hpp`, `app/include/Updater.hpp`. It changed the repository support state, metadata, or supporting files in the way described by `chore(app): tighten app constructors`.

Before this commit, the repository reflected the state immediately preceding `9e8d042`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/DatabaseManager.hpp b/app/include/DatabaseManager.hpp
--- a/app/include/DatabaseManager.hpp
+++ b/app/include/DatabaseManager.hpp
@@ -10,7 +10,7 @@
 
 class DatabaseManager {
 public:
-    DatabaseManager(std::string config_dir);
+    explicit DatabaseManager(std::string config_dir);
     ~DatabaseManager();
 
     bool is_file_already_categorized(const std::string &file_name);
diff --git a/app/include/LLMClient.hpp b/app/include/LLMClient.hpp
index 315e091..9c131c2 100644
--- a/app/include/LLMClient.hpp
+++ b/app/include/LLMClient.hpp
@@ -7,7 +7,7 @@
 
 class LLMClient : public ILLMClient {
 public:
-    LLMClient(const std::string &api_key);
+    explicit LLMClient(const std::string &api_key);
     ~LLMClient() override;
     std::string categorize_file(const std::string& file_name,
                                 const std::string& file_path,
```

The excerpt is taken from the commit diff for `chore(app): tighten app constructors`. The most relevant surfaces are `app/include/DatabaseManager.hpp`, `app/include/LLMClient.hpp`, `app/include/LLMDownloader.hpp`, `app/include/Updater.hpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
