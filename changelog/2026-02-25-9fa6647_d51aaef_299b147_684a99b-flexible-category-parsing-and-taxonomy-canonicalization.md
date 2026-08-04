# 2026-02-25: Flexible category parsing and taxonomy canonicalization to reduce duplicate folders

## Covered commits
- `9fa6647` `2026-02-25` `feat(categorization): parse flexible LLM category/subcategory output formats`
- `d51aaef` `2026-02-25` `feat(taxonomy): canonicalize common category synonyms to reduce duplicate folders`
- `299b147` `2026-02-26` `feat(categorization): parse flexible category/subcategory LLM output formats`
- `684a99b` `2026-02-26` `feat(database): canonicalize category synonyms for taxonomy matching`

## Motivation
LLMs were producing categories and subcategories in more formats than the parser initially expected, and near-synonymous category spellings were causing duplicate folders. The categorization pipeline needed to accept flexible output while still canonicalizing labels aggressively enough to keep the folder tree tidy.

## What changed
These commits broadened category/subcategory parsing, introduced canonicalization of common synonyms both in the taxonomy layer and in persisted database results, and laid the groundwork for the later media-rename and multilingual-label hardening.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `9fa6647`
```diff
diff --git a/app/lib/CategorizationService.cpp b/app/lib/CategorizationService.cpp
--- a/app/lib/CategorizationService.cpp
+++ b/app/lib/CategorizationService.cpp
@@ -26,18 +26,133 @@ constexpr const char* kRemoteTimeoutEnv = "AI_FILE_SORTER_REMOTE_LLM_TIMEOUT";
 constexpr const char* kCustomTimeoutEnv = "AI_FILE_SORTER_CUSTOM_LLM_TIMEOUT";
 constexpr size_t kMaxConsistencyHints = 5;
 constexpr size_t kMaxLabelLength = 80;
+std::string to_lower_copy_str(std::string value);
 
-// Splits a "Category : Subcategory" string and sanitizes the labels.
+std::string trim_copy(std::string value) {
+    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
+    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
+    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
+    return value;
+}
+
+std::string strip_list_prefix(std::string line) {
+    line = trim_copy(std::move(line));
+    if (line.empty()) {
+        return line;
+    }
+
+    if ((line.front() == '-' || line.front() == '*') && line.size() > 1 &&
+        std::isspace(static_cast<unsigned char>(line[1]))) {
+        return trim_copy(line.substr(1));
+    }
+
+    size_t idx = 0;
+    while (idx < line.size() && std::isdigit(static_cast<unsigned char>(line[idx]))) {
+        ++idx;
+    }
+    if (idx > 0 && idx + 1 < line.size() &&
+        (line[idx] == '.' || line[idx] == ')') &&
+        std::isspace(static_cast<unsigned char>(line[idx + 1]))) {
+        return trim_copy(line.substr(idx + 1));
+    }
+
+    return line;
+}
+
+bool has_alpha(const std::string& value) {
+    return std::any_of(value.begin(), value.end(), [](unsigned char ch) {
+        return std::isalpha(ch);
+    });
+}
+
+bool split_inline_pair(const std::string& line, std::string& category, std::string& subcategory) {
+    for (const std::string delimiter : {std::string(" : "), std::string(":")}) {
+        const auto pos = line.find(delimiter);
+        if (pos == std::string::npos) {
+            continue;
+        }
+        std::string left = trim_copy(line.substr(0, pos));
+        std::string right = trim_copy(line.substr(pos + delimiter.size()));
+        if (left.size() < 2 || right.empty()) {
+            continue;
+        }
+        if (!has_alpha(left) || !has_alpha(right)) {
+            continue;
+        }
+        category = left;
+        subcategory = right;
+        return true;
+    }
+    return false;
+}
+
+// Splits common category/subcategory response variants and sanitizes the labels.
 std::pair<std::string, std::string> split_category_subcategory(const std::string& input) {
-    const std::string delimiter = " : ";
+    std::vector<std::string> lines;
+    lines.reserve(4);
+
+    std::istringstream iss(input);
+    std::string line;
+    while (std::getline(iss, line)) {
+        std::string cleaned = strip_list_prefix(std::move(line));
+        if (!cleaned.empty()) {
+            lines.push_back(std::move(cleaned));
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `d51aaef`
```diff
diff --git a/app/lib/DatabaseManager.cpp b/app/lib/DatabaseManager.cpp
--- a/app/lib/DatabaseManager.cpp
+++ b/app/lib/DatabaseManager.cpp
@@ -446,6 +446,126 @@ static std::string strip_trailing_stopwords(const std::string& normalized) {
     return joined;
 }
 
+struct CanonicalCategoryLabel {
+    std::string normalized;
+    std::string display;
+};
+
+bool is_image_like_label(const std::string& normalized) {
+    if (normalized.empty()) {
+        return false;
+    }
+    static const std::unordered_set<std::string> kImageLike = {
+        "image", "images",
+        "image file", "image files",
+        "photo", "photos",
+        "graphic", "graphics",
+        "picture", "pictures",
+        "pic", "pics",
+        "screenshot", "screenshots",
+        "wallpaper", "wallpapers"
+    };
+    if (kImageLike.contains(normalized)) {
+        return true;
+    }
+    return kImageLike.contains(strip_trailing_stopwords(normalized));
+}
+
+CanonicalCategoryLabel canonicalize_category_label(const std::string& normalized_category,
+                                                   const std::string& normalized_subcategory) {
+    static const std::unordered_map<std::string, CanonicalCategoryLabel> kCategorySynonyms = {
+        {"archive", {"archives", "Archives"}},
+        {"archives", {"archives", "Archives"}},
+        {"backup", {"archives", "Archives"}},
+        {"backups", {"archives", "Archives"}},
+        {"backup file", {"archives", "Archives"}},
+        {"backup files", {"archives", "Archives"}},
+
+        {"document", {"documents", "Documents"}},
+        {"documents", {"documents", "Documents"}},
+        {"doc", {"documents", "Documents"}},
+        {"docs", {"documents", "Documents"}},
+        {"text", {"documents", "Documents"}},
+        {"texts", {"documents", "Documents"}},
+        {"paper", {"documents", "Documents"}},
+        {"papers", {"documents", "Documents"}},
+        {"report", {"documents", "Documents"}},
+        {"reports", {"documents", "Documents"}},
+        {"spreadsheet", {"documents", "Documents"}},
+        {"spreadsheets", {"documents", "Documents"}},
+        {"table", {"documents", "Documents"}},
+        {"tables", {"documents", "Documents"}},
+        {"office file", {"documents", "Documents"}},
+        {"office files", {"documents", "Documents"}},
+
+        {"software", {"software", "Software"}},
+        {"application", {"software", "Software"}},
+        {"applications", {"software", "Software"}},
+        {"app", {"software", "Software"}},
+        {"apps", {"software", "Software"}},
+        {"program", {"software", "Software"}},
+        {"programs", {"software", "Software"}},
+        {"installer", {"software", "Software"}},
+        {"installers", {"software", "Software"}},
+        {"installation", {"software", "Software"}},
+        {"installations", {"software", "Software"}},
+        {"installation file", {"software", "Software"}},
+        {"installation files", {"software", "Software"}},
+        {"software installation", {"software", "Software"}},
+        {"software installations", {"software", "Software"}},
+        {"software installation file", {"software", "Software"}},
+        {"software installation files", {"software", "Software"}},
+        {"setup", {"software", "Software"}},
+        {"setups", {"software", "Software"}},
+        {"setup file", {"software", "Software"}},
+        {"setup files", {"software", "Software"}},
```

This second excerpt is included because `d51aaef` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
