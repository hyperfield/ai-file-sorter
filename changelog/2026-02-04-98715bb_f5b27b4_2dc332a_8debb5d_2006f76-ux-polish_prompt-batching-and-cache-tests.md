# 2026-02-04: UX polish, prompt batching, unicode-safe rename handling, and cache tests

## Covered commits
- `98715bb` `2026-02-04` `feat(benchmark): add suppress-checkbox for auto-show dialog`
- `f5b27b4` `2026-02-04` `fix(rename): keep unicode-safe sanitization for suggestions`
- `2dc332a` `2026-02-04` `feat(ui): update edit-tip copy with icon and translations`
- `8debb5d` `2026-02-04` `fix(llm): batch prompt decoding to respect n_batch`
- `2006f76` `2026-02-04` `test(cache): cover cache interactions and normalize scan paths`
- `1da581a` `2026-02-04` `fix(llm-selection-dialog): fix dialog appearance condition`

## Motivation
The same release window also contained a set of important quality fixes that were smaller than the macOS build work but highly visible to users. The benchmark dialog auto-show behavior, rename sanitization, edit-tip wording, prompt batching, and cache behavior all needed tightening so the app felt coherent rather than experimental.

## What changed
This grouped slice added a suppress checkbox for the benchmark dialog, preserved unicode-safe rename sanitization, refreshed edit-tip copy and translations, fixed prompt decoding to respect `n_batch`, expanded cache tests, and cleaned up a dialog appearance condition in the LLM selection UI.

In practical terms, the grouped commits moved the project from the previous behavior to a more explicit, more testable, and more user-facing implementation. Where a later commit in the list is smaller than the first one, it is still included because it completed or corrected the same logical change set rather than standing on its own as a separate product chapter.

## Representative excerpt: `98715bb`
```diff
diff --git a/app/include/Settings.hpp b/app/include/Settings.hpp
--- a/app/include/Settings.hpp
+++ b/app/include/Settings.hpp
@@ -217,11 +217,21 @@ public:
      * @return True when the benchmark has run at least once.
      */
     bool get_suitability_benchmark_completed() const;
+    /**
+     * @brief Returns whether the suitability benchmark dialog is suppressed.
+     * @return True when the dialog should not auto-show on startup.
+     */
+    bool get_suitability_benchmark_suppressed() const;
     /**
      * @brief Marks the suitability benchmark as completed.
      * @param value True when the benchmark has run.
      */
     void set_suitability_benchmark_completed(bool value);
+    /**
+     * @brief Sets whether the suitability benchmark dialog is suppressed.
+     * @param value True to suppress auto-showing the dialog.
+     */
+    void set_suitability_benchmark_suppressed(bool value);
     /**
      * @brief Returns the most recent benchmark report text.
      * @return Report text (may be empty).
@@ -305,6 +315,7 @@ private:
     std::string skipped_version;
     bool show_file_explorer{true};
     bool suitability_benchmark_completed{false};
+    bool suitability_benchmark_suppressed{false};
     std::string benchmark_last_report;
     std::string benchmark_last_run;
     Language language{Language::English};
```

The excerpt above is representative of the primary commit in this chapter: it shows the code surface where the behavior changed most visibly. The surrounding follow-up commits in the same group either hardened the implementation, extended tests, adjusted documentation, or corrected cross-platform edge cases introduced by the main change.

## Representative excerpt: `f5b27b4`
```diff
diff --git a/app/lib/DocumentTextAnalyzer.cpp b/app/lib/DocumentTextAnalyzer.cpp
--- a/app/lib/DocumentTextAnalyzer.cpp
+++ b/app/lib/DocumentTextAnalyzer.cpp
@@ -76,19 +76,25 @@ std::string collapse_whitespace(const std::string& value) {
     return trim_copy(collapsed);
 }
 
-std::vector<std::string> split_words(const std::string& value) {
+QString sanitize_utf8_text(const std::string& value) {
+    QString cleaned = QString::fromUtf8(value.c_str());
+    cleaned.remove(QChar::ReplacementCharacter);
+    return cleaned.normalized(QString::NormalizationForm_C);
+}
+
+std::vector<std::string> split_words(const QString& value) {
     std::vector<std::string> words;
-    std::string current;
-    for (unsigned char ch : value) {
-        if (std::isalnum(ch)) {
-            current.push_back(static_cast<char>(std::tolower(ch)));
-        } else if (!current.empty()) {
-            words.emplace_back(std::move(current));
+    QString current;
+    for (const QChar ch : value) {
+        if (ch.isLetterOrNumber()) {
+            current.append(ch.toLower());
+        } else if (!current.isEmpty()) {
+            words.emplace_back(current.toUtf8().toStdString());
             current.clear();
         }
     }
-    if (!current.empty()) {
-        words.emplace_back(std::move(current));
+    if (!current.isEmpty()) {
+        words.emplace_back(current.toUtf8().toStdString());
     }
     return words;
 }
@@ -673,19 +679,25 @@ std::string DocumentTextAnalyzer::build_prompt(const std::string& excerpt,
 std::string DocumentTextAnalyzer::sanitize_filename(const std::string& value,
                                                     size_t max_words,
                                                     size_t max_length) const {
-    std::string cleaned = trim_copy(value);
-    const std::string lower = to_lower_copy(cleaned);
-    const std::string prefix = "filename:";
-    if (lower.rfind(prefix, 0) == 0) {
-        cleaned = trim_copy(cleaned.substr(prefix.size()));
+    QString cleaned = sanitize_utf8_text(value).trimmed();
+    const QString prefix = QStringLiteral("filename:");
+    if (cleaned.startsWith(prefix, Qt::CaseInsensitive)) {
+        cleaned = cleaned.mid(prefix.size()).trimmed();
     }
-    const auto newline = cleaned.find('\n');
-    if (newline != std::string::npos) {
-        cleaned = cleaned.substr(0, newline);
+    const int newline = cleaned.indexOf('\n');
+    if (newline != -1) {
+        cleaned = cleaned.left(newline);
     }
-    if (cleaned.size() >= 2 && ((cleaned.front() == '"' && cleaned.back() == '"') ||
-                                (cleaned.front() == '\'' && cleaned.back() == '\''))) {
-        cleaned = cleaned.substr(1, cleaned.size() - 2);
+    const int carriage = cleaned.indexOf('\r');
+    if (carriage != -1) {
+        cleaned = cleaned.left(carriage);
+    }
+    if (cleaned.size() >= 2) {
+        const QChar first = cleaned.front();
+        const QChar last = cleaned.back();
+        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
+            cleaned = cleaned.mid(1, cleaned.size() - 2);
+        }
     }
 
     auto words = split_words(cleaned);
```

This second excerpt is included because `f5b27b4` was part of the same logical change set and captures either the test, packaging, portability, or UI follow-up that justified keeping the commits together in one chapter.

## Rationale for grouping
The commits listed here are grouped intentionally rather than split into one file per commit because they describe one user-visible or architecture-visible change train. Later commits in the same group are narrow follow-ups such as tests, packaging, translations, documentation, or platform-specific corrections for the same feature, which matches the grouping rule in `changelog/changelog_instructions.md`.

## Before vs. after
Before this chapter's work, the relevant feature area was either missing, incomplete, or fragile on at least one supported platform. After the grouped commits, the feature or fix became part of the normal supported product path, with the related tests, docs, or cross-platform adjustments landing close enough in time that splitting them into separate chapters would make the history harder to follow rather than easier.

## Notes
This chapter was generated as part of a local backlog backfill for the ignored `changelog/` directory. The covered-commit list above is the authoritative mapping for matching Git history to this chapter later.
