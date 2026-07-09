# 2026-03-01: docs(settings): document the public Settings API

## Covered commits
- `0e47bb0` `2026-03-01` `docs(settings): document the public Settings API`

## Motivation
This documentation-focused commit kept the written project state aligned with the code, packaging, or release state that existed at that point in history. Without it, contributors and users would have been looking at stale instructions, screenshots, or release notes.

## Commit message body
The commit message did not include a longer body beyond the subject line.

## Files changed
- `M` `app/include/Settings.hpp`

## What changed from what, why, and how
The commit updated documentation artifacts touching `app/include/Settings.hpp`. It moved the repository from an older written or illustrated state to one that matched the implementation and release state of the time.

Before this commit, the repository reflected the state immediately preceding `0e47bb0`. After this commit, the files listed above incorporated the exact delta shown in the excerpt and file list, which is why this entry is stored as its own chapter instead of being folded into a broader release note.

## Relevant excerpt with comments
```diff
diff --git a/app/include/Settings.hpp b/app/include/Settings.hpp
--- a/app/include/Settings.hpp
+++ b/app/include/Settings.hpp
@@ -10,74 +10,221 @@
 #include <vector>
 #include <functional>
 
-
+/**
+ * @brief Stores and persists application configuration for UI and runtime behavior.
+ */
 class Settings
 {
 public:
+    /**
+     * @brief Constructs a settings object with platform-appropriate defaults.
+     */
     Settings();
 
+    /**
+     * @brief Loads configuration values from the active config file.
+     * @return True when an existing config file was loaded successfully.
+     */
     bool load();
+    /**
+     * @brief Persists current configuration values to the active config file.
+     * @return True when the config file was written successfully.
+     */
     bool save();
 
+    /**
+     * @brief Returns the selected LLM choice.
+     * @return Current LLM choice enum.
+     */
     LLMChoice get_llm_choice() const;
+    /**
+     * @brief Sets the selected LLM choice.
+     * @param choice LLM choice to store.
+     */
     void set_llm_choice(LLMChoice choice);
+    /**
+     * @brief Returns the stored OpenAI API key.
+     * @return OpenAI API key string.
+     */
     std::string get_openai_api_key() const;
+    /**
+     * @brief Stores the OpenAI API key.
+     * @param key OpenAI API key text.
+     */
     void set_openai_api_key(const std::string& key);
+    /**
+     * @brief Returns the configured OpenAI model identifier.
+     * @return OpenAI model name.
+     */
     std::string get_openai_model() const;
+    /**
+     * @brief Sets the OpenAI model identifier.
+     * @param model OpenAI model name to store.
+     */
     void set_openai_model(const std::string& model);
+    /**
+     * @brief Returns the stored Gemini API key.
+     * @return Gemini API key string.
+     */
     std::string get_gemini_api_key() const;
+    /**
+     * @brief Stores the Gemini API key.
+     * @param key Gemini API key text.
+     */
     void set_gemini_api_key(const std::string& key);
+    /**
+     * @brief Returns the configured Gemini model identifier.
+     * @return Gemini model name.
+     */
     std::string get_gemini_model() const;
+    /**
+     * @brief Sets the Gemini model identifier.
+     * @param model Gemini model name to store.
+     */
     void set_gemini_model(const std::string& model);
+    /**
+     * @brief Returns whether the LLM download UI section should remain expanded.
+     * @return True when the downloads section is expanded.
+     */
     bool get_llm_downloads_expanded() const;
+    /**
+     * @brief Sets whether the LLM download UI section should remain expanded.
+     * @param value True to keep the downloads section expanded.
+     */
     void set_llm_downloads_expanded(bool value);
+    /**
+     * @brief Returns the configured output language for categories.
+     * @return Selected category language.
+     */
     CategoryLanguage get_category_language() const;
+    /**
+     * @brief Sets the output language for categories.
+     * @param language Category language to store.
+     */
     void set_category_language(CategoryLanguage language);
```

The excerpt is taken from the commit diff for `docs(settings): document the public Settings API`. The most relevant surfaces are `app/include/Settings.hpp`; those lines show where the repository crossed from the previous implementation or documentation state into the new one.

## Notes
This entry was generated to close a backlog gap in the local ignored `changelog/` directory. The filename includes the short commit id so future matching between Git history and changelog files stays mechanical.
