# 2025-08-06: chore(logger): change log level to debug

## Covered commits
- `ea3ecdf` `2025-08-06` `chore(logger): change log level to debug`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/Logger.cpp`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/lib/Logger.cpp`. It changed the repository support state, metadata, or supporting files in the way described by `chore(logger): change log level to debug`.

Before this commit, the repository reflected the state immediately preceding `ea3ecdf`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/Logger.cpp b/app/lib/Logger.cpp
--- a/app/lib/Logger.cpp
+++ b/app/lib/Logger.cpp
@@ -66,7 +66,7 @@ void Logger::setup_loggers()
     spdlog::register_logger(db_logger);
     spdlog::register_logger(ui_logger);
 
-    spdlog::set_level(spdlog::level::warn);
+    spdlog::set_level(spdlog::level::debug);
     spdlog::info("Loggers initialized.");
 }
```

The excerpt is taken from the commit diff for `chore(logger): change log level to debug`. The most relevant surfaces are `app/lib/Logger.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
