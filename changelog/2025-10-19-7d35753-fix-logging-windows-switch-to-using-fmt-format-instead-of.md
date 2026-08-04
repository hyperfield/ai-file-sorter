# 2025-10-19: fix(logging-windows): switch to using fmt::format instead of fmt::make_format_args

## Covered commits
- `7d35753` `2025-10-19` `fix(logging-windows): switch to using fmt::format instead of fmt::make_format_args`

## Motivation
This fix commit existed because the previous behavior was wrong, incomplete, or brittle for at least one real usage path. The change was meant to close that gap directly rather than introduce a new feature surface.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/DatabaseManager.cpp`
- `M` `app/lib/IniConfig.cpp`
- `M` `app/lib/Settings.cpp`
- `M` `app/lib/Updater.cpp`
- `M` `app/lib/Utils.cpp`

## What changed from what, why, and how
The commit corrected behavior in `app/lib/DatabaseManager.cpp`, `app/lib/IniConfig.cpp`, `app/lib/Settings.cpp`, `app/lib/Updater.cpp`, `app/lib/Utils.cpp`. It changed the implementation from the previous faulty or fragile path to the repaired path described by the subject line `fix(logging-windows): switch to using fmt::format instead of fmt::make_format_args`.

Before this commit, the repository reflected the state immediately preceding `7d35753`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/DatabaseManager.cpp b/app/lib/DatabaseManager.cpp
--- a/app/lib/DatabaseManager.cpp
+++ b/app/lib/DatabaseManager.cpp
@@ -21,7 +21,7 @@ constexpr double kSimilarityThreshold = 0.85;
 
 template <typename... Args>
 void db_log(spdlog::level::level_enum level, const char* fmt, Args&&... args) {
-    auto message = fmt::vformat(fmt, fmt::make_format_args(std::forward<Args>(args)...));
+    auto message = fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...);
     if (auto logger = Logger::get_logger("core_logger")) {
         logger->log(level, "{}", message);
     } else {
diff --git a/app/lib/IniConfig.cpp b/app/lib/IniConfig.cpp
index 46d0aec..b1eadb8 100644
--- a/app/lib/IniConfig.cpp
+++ b/app/lib/IniConfig.cpp
@@ -8,7 +8,7 @@
 namespace {
 template <typename... Args>
 void ini_log(spdlog::level::level_enum level, const char* fmt, Args&&... args) {
-    auto message = fmt::vformat(fmt, fmt::make_format_args(std::forward<Args>(args)...));
+    auto message = fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...);
     if (auto logger = Logger::get_logger("core_logger")) {
         logger->log(level, "{}", message);
     } else {
```

The excerpt is taken from the commit diff for `fix(logging-windows): switch to using fmt::format instead of fmt::make_format_args`. The most relevant surfaces are `app/lib/DatabaseManager.cpp`, `app/lib/IniConfig.cpp`, `app/lib/Settings.cpp`, `app/lib/Updater.cpp`, `app/lib/Utils.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
