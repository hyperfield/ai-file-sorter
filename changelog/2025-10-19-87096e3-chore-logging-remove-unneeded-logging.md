# 2025-10-19: chore(logging): remove unneeded logging

## Covered commits
- `87096e3` `2025-10-19` `chore(logging): remove unneeded logging`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/EmbeddedEnv.cpp`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/lib/EmbeddedEnv.cpp`. It changed the repository support state, metadata, or supporting files in the way described by `chore(logging): remove unneeded logging`.

Before this commit, the repository reflected the state immediately preceding `87096e3`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/EmbeddedEnv.cpp b/app/lib/EmbeddedEnv.cpp
--- a/app/lib/EmbeddedEnv.cpp
+++ b/app/lib/EmbeddedEnv.cpp
@@ -17,14 +17,14 @@ EmbeddedEnv::EmbeddedEnv(const std::string& resource_path)
 void EmbeddedEnv::load_env() {
     auto logger = Logger::get_logger("core_logger");
     if (logger) {
-        logger->debug("Loading embedded environment from {}", resource_path_);
+        // logger->debug("Loading embedded environment from {}", resource_path_);
     }
 
     std::string env_content = extract_env_content();
     parse_env(env_content);
 
     if (logger) {
-        logger->info("Embedded environment loaded from {}", resource_path_);
+        // logger->info("Embedded environment loaded from {}", resource_path_);
     }
 }
```

The excerpt is taken from the commit diff for `chore(logging): remove unneeded logging`. The most relevant surfaces are `app/lib/EmbeddedEnv.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
