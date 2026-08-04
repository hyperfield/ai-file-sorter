# 2025-11-01: chore(utils): reduce cyclomatic complexity

## Covered commits
- `6ef50fc` `2025-11-01` `chore(utils): reduce cyclomatic complexity`

## Motivation
This maintenance commit kept the repository state coherent even though it was not a headline feature. Chore commits often synchronize metadata, ignore rules, repository hygiene, or supporting assets that later work depends on.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/lib/Utils.cpp`

## What changed from what, why, and how
The commit made maintenance-oriented updates in `app/lib/Utils.cpp`. It changed the repository support state, metadata, or supporting files in the way described by `chore(utils): reduce cyclomatic complexity`.

Before this commit, the repository reflected the state immediately preceding `6ef50fc`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/lib/Utils.cpp b/app/lib/Utils.cpp
--- a/app/lib/Utils.cpp
+++ b/app/lib/Utils.cpp
@@ -29,6 +29,81 @@ void log_core(spdlog::level::level_enum level, const char* fmt, Args&&... args)
         std::fprintf(stderr, "%s\n", message.c_str());
     }
 }
+
+std::string to_forward_slashes(std::string value) {
+    std::replace(value.begin(), value.end(), '\\', '/');
+    return value;
+}
+
+std::string trim_leading_separators(std::string value) {
+    auto is_separator = [](char ch) {
+        return ch == '/' || ch == '\\';
+    };
+    value.erase(value.begin(), std::find_if(value.begin(), value.end(),
+        [&](char ch) { return !is_separator(ch); }));
+    return value;
+}
+
+std::optional<std::filesystem::path> try_utf8_to_path(const std::string& value) {
+    try {
+        return Utils::utf8_to_path(value);
+    } catch (const std::exception&) {
+        return std::nullopt;
+    }
+}
+
+std::vector<std::string> collect_user_prefixes() {
+    std::vector<std::string> prefixes;
+
+    auto append = [&](const char* candidate) {
+        if (!candidate || *candidate == '\0') {
+            return;
+        }
+        const std::string raw(candidate);
+        if (auto converted = try_utf8_to_path(raw)) {
+            prefixes.push_back(to_forward_slashes(Utils::path_to_utf8(*converted)));
+        } else {
+            prefixes.push_back(to_forward_slashes(raw));
+        }
+    };
+
+    append(std::getenv("HOME"));
+    append(std::getenv("USERPROFILE"));
+
+    if (prefixes.empty() && Utils::is_os_windows()) {
+        if (const char* username = std::getenv("USERNAME")) {
+            prefixes.emplace_back(std::string("C:/Users/") + username);
+        }
+    }
+
+    return prefixes;
+}
+
+std::optional<std::string> strip_prefix(const std::string& path,
+                                        const std::vector<std::string>& prefixes) {
+    for (const auto& original_prefix : prefixes) {
+        if (original_prefix.empty()) {
+            continue;
+        }
+        std::string prefix = original_prefix;
+        if (prefix.back() != '/') {
+            prefix.push_back('/');
+        }
+        if (path.size() < prefix.size()) {
+            continue;
+        }
+        if (!std::equal(prefix.begin(), prefix.end(), path.begin())) {
+            continue;
+        }
+
+        std::string trimmed = trim_leading_separators(path.substr(prefix.size()));
+        if (!trimmed.empty()) {
+            return trimmed;
+        }
+    }
+
+    return std::nullopt;
+}
 }
 #ifdef _WIN32
     #include <windows.h>
@@ -650,77 +725,20 @@ std::string Utils::abbreviate_user_path(const std::string& path) {
         return "";
     }
 
-    auto to_forward_slashes = [](std::string value) {
-        std::replace(value.begin(), value.end(), '\\', '/');
-        return value;
-    };
-
-    std::filesystem::path fs_path;
-    try {
-        fs_path = Utils::utf8_to_path(path);
-    } catch (const std::exception&) {
-        std::string fallback = to_forward_slashes(path);
-        while (!fallback.empty() && (fallback.front() == '/' || fallback.front() == '\\')) {
```

The excerpt is taken from the commit diff for `chore(utils): reduce cyclomatic complexity`. The most relevant surfaces are `app/lib/Utils.cpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
