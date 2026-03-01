#ifndef SETTINGS_HPP
#define SETTINGS_HPP

#include <IniConfig.hpp>
#include <Types.hpp>
#include <Language.hpp>
#include <CategoryLanguage.hpp>
#include <string>
#include <filesystem>
#include <vector>
#include <functional>

/**
 * @brief Stores and persists application configuration for UI and runtime behavior.
 */
class Settings
{
public:
    /**
     * @brief Constructs a settings object with platform-appropriate defaults.
     */
    Settings();

    /**
     * @brief Loads configuration values from the active config file.
     * @return True when an existing config file was loaded successfully.
     */
    bool load();
    /**
     * @brief Persists current configuration values to the active config file.
     * @return True when the config file was written successfully.
     */
    bool save();

    /**
     * @brief Returns the selected LLM choice.
     * @return Current LLM choice enum.
     */
    LLMChoice get_llm_choice() const;
    /**
     * @brief Sets the selected LLM choice.
     * @param choice LLM choice to store.
     */
    void set_llm_choice(LLMChoice choice);
    /**
     * @brief Returns the stored OpenAI API key.
     * @return OpenAI API key string.
     */
    std::string get_openai_api_key() const;
    /**
     * @brief Stores the OpenAI API key.
     * @param key OpenAI API key text.
     */
    void set_openai_api_key(const std::string& key);
    /**
     * @brief Returns the configured OpenAI model identifier.
     * @return OpenAI model name.
     */
    std::string get_openai_model() const;
    /**
     * @brief Sets the OpenAI model identifier.
     * @param model OpenAI model name to store.
     */
    void set_openai_model(const std::string& model);
    /**
     * @brief Returns the stored Gemini API key.
     * @return Gemini API key string.
     */
    std::string get_gemini_api_key() const;
    /**
     * @brief Stores the Gemini API key.
     * @param key Gemini API key text.
     */
    void set_gemini_api_key(const std::string& key);
    /**
     * @brief Returns the configured Gemini model identifier.
     * @return Gemini model name.
     */
    std::string get_gemini_model() const;
    /**
     * @brief Sets the Gemini model identifier.
     * @param model Gemini model name to store.
     */
    void set_gemini_model(const std::string& model);
    /**
     * @brief Returns whether the LLM download UI section should remain expanded.
     * @return True when the downloads section is expanded.
     */
    bool get_llm_downloads_expanded() const;
    /**
     * @brief Sets whether the LLM download UI section should remain expanded.
     * @param value True to keep the downloads section expanded.
     */
    void set_llm_downloads_expanded(bool value);
    /**
     * @brief Returns the configured output language for categories.
     * @return Selected category language.
     */
    CategoryLanguage get_category_language() const;
    /**
     * @brief Sets the output language for categories.
     * @param language Category language to store.
     */
    void set_category_language(CategoryLanguage language);
    /**
     * @brief Returns the active custom local LLM identifier.
     * @return Custom LLM id, or empty when none is selected.
     */
    std::string get_active_custom_llm_id() const;
    /**
     * @brief Sets the active custom local LLM identifier.
     * @param id Custom LLM id to store.
     */
    void set_active_custom_llm_id(const std::string& id);
    /**
     * @brief Returns the configured custom local LLM entries.
     * @return Immutable list of configured custom LLMs.
     */
    const std::vector<CustomLLM>& get_custom_llms() const;
    /**
     * @brief Adds or updates a custom local LLM entry.
     * @param llm Custom LLM definition to upsert.
     * @return Identifier of the saved custom LLM entry.
     */
    std::string upsert_custom_llm(const CustomLLM& llm);
    /**
     * @brief Removes a custom local LLM entry by id.
     * @param id Custom LLM identifier to remove.
     */
    void remove_custom_llm(const std::string& id);
    /**
     * @brief Finds a custom local LLM entry by id.
     * @param id Custom LLM identifier to resolve.
     * @return Matching custom LLM entry, or a default-initialized value when not found.
     */
    CustomLLM find_custom_llm(const std::string& id) const;
    /**
     * @brief Return the active custom API endpoint id.
     * @return Custom API endpoint id, or empty when none is selected.
     */
    std::string get_active_custom_api_id() const;
    /**
     * @brief Set the active custom API endpoint id.
     * @param id Custom API endpoint id to store.
     */
    void set_active_custom_api_id(const std::string& id);
    /**
     * @brief Return the configured custom API endpoints.
     * @return Immutable list of configured custom API endpoints.
     */
    const std::vector<CustomApiEndpoint>& get_custom_api_endpoints() const;
    /**
     * @brief Add or update a custom API endpoint entry.
     * @param endpoint Custom endpoint definition to upsert.
     * @return Identifier of the saved custom endpoint entry.
     */
    std::string upsert_custom_api_endpoint(const CustomApiEndpoint& endpoint);
    /**
     * @brief Remove a custom API endpoint by id.
     * @param id Custom API endpoint identifier to remove.
     */
    void remove_custom_api_endpoint(const std::string& id);
    /**
     * @brief Find a custom API endpoint by id.
     * @param id Custom API endpoint identifier to resolve.
     * @return Matching custom API endpoint, or a default-initialized value when not found.
     */
    CustomApiEndpoint find_custom_api_endpoint(const std::string& id) const;
    /**
     * @brief Returns whether an LLM choice has been configured.
     * @return True when the stored LLM choice is not unset.
     */
    bool is_llm_chosen() const;

    /**
     * @brief Returns whether subcategories are enabled.
     * @return True when subcategories should be used.
     */
    bool get_use_subcategories() const;
    /**
     * @brief Enables or disables subcategories.
     * @param value True to enable subcategories.
     */
    void set_use_subcategories(bool value);

    /**
     * @brief Returns whether consistency hints are enabled.
     * @return True when consistency hints should be used.
     */
    bool get_use_consistency_hints() const;
    /**
     * @brief Enables or disables consistency hints.
     * @param value True to enable consistency hints.
     */
    void set_use_consistency_hints(bool value);

    /**
     * @brief Returns whether files should be categorized.
     * @return True when file categorization is enabled.
     */
    bool get_categorize_files() const;
    /**
     * @brief Enables or disables file categorization.
     * @param value True to categorize files.
     */
    void set_categorize_files(bool value);

    /**
     * @brief Returns whether directories should be categorized.
     * @return True when directory categorization is enabled.
     */
    bool get_categorize_directories() const;
    /**
     * @brief Enables or disables directory categorization.
     * @param value True to categorize directories.
     */
    void set_categorize_directories(bool value);

    /**
     * @brief Returns whether subdirectories should be scanned.
     * @return True when subdirectory scanning is enabled.
     */
    bool get_include_subdirectories() const;
    /**
     * @brief Enables or disables scanning of subdirectories.
     * @param value True to scan subdirectories.
     */
    void set_include_subdirectories(bool value);

    /**
     * @brief Returns whether image content analysis is enabled.
     * @return True when image analysis is enabled.
     */
    bool get_analyze_images_by_content() const;
    /**
     * @brief Enables or disables image content analysis.
     * @param value True to enable image analysis.
     */
    void set_analyze_images_by_content(bool value);
    /**
     * @brief Returns whether image rename suggestions are enabled.
     * @return True when image rename suggestions are enabled.
     */
    bool get_offer_rename_images() const;
    /**
     * @brief Enables or disables image rename suggestions.
     * @param value True to enable image rename suggestions.
     */
    void set_offer_rename_images(bool value);
    /**
     * @brief Returns whether image filename suggestions should include EXIF date/place prefixes.
     * @return True when EXIF date/place prefixes are enabled for image rename suggestions.
     */
    bool get_add_image_date_place_to_filename() const;
    /**
     * @brief Enables or disables adding EXIF date/place prefixes to image rename suggestions.
     * @param value True to enable EXIF date/place prefixes.
     */
    void set_add_image_date_place_to_filename(bool value);
    /**
     * @brief Returns whether audio/video filename suggestions should include media metadata.
     * @return True when audio/video metadata-based filename suggestions are enabled.
     */
    bool get_add_audio_video_metadata_to_filename() const;
    /**
     * @brief Enables or disables audio/video metadata-based filename suggestions.
     * @param value True to enable metadata-based filename suggestions for audio/video files.
     */
    void set_add_audio_video_metadata_to_filename(bool value);
    /**
     * @brief Returns whether image creation dates should be appended to category names.
     * @return True when image creation dates should be appended to categories.
     */
    bool get_add_image_date_to_category() const;
    /**
     * @brief Enables or disables appending image creation dates to category names.
     * @param value True to append image creation dates to categories.
     */
    void set_add_image_date_to_category(bool value);
    /**
     * @brief Returns whether the image options group is expanded.
     * @return True when the image options group should be expanded.
     */
    bool get_image_options_expanded() const;
    /**
     * @brief Sets whether the image options group is expanded.
     * @param value True to keep the image options group expanded.
     */
    void set_image_options_expanded(bool value);
    /**
     * @brief Returns whether image files are treated as rename-only.
     * @return True when image files are in rename-only mode.
     */
    bool get_rename_images_only() const;
    /**
     * @brief Enables or disables rename-only mode for images.
     * @param value True to enable rename-only mode for images.
     */
    void set_rename_images_only(bool value);
    /**
     * @brief Returns whether only image files are processed.
     * @return True when only image files are processed.
     */
    bool get_process_images_only() const;
    /**
     * @brief Enables or disables image-only processing.
     * @param value True to enable image-only processing.
     */
    void set_process_images_only(bool value);
    /**
     * @brief Returns whether document content analysis is enabled.
     * @return True when document analysis is enabled.
     */
    bool get_analyze_documents_by_content() const;
    /**
     * @brief Enables or disables document content analysis.
     * @param value True to enable document analysis.
     */
    void set_analyze_documents_by_content(bool value);
    /**
     * @brief Returns whether document rename suggestions are enabled.
     * @return True when document rename suggestions are enabled.
     */
    bool get_offer_rename_documents() const;
    /**
     * @brief Enables or disables document rename suggestions.
     * @param value True to enable document rename suggestions.
     */
    void set_offer_rename_documents(bool value);
    /**
     * @brief Returns whether the document options group is expanded.
     * @return True when the document options group should be expanded.
     */
    bool get_document_options_expanded() const;
    /**
     * @brief Sets whether the document options group is expanded.
     * @param value True to keep the document options group expanded.
     */
    void set_document_options_expanded(bool value);
    /**
     * @brief Returns whether document files are treated as rename-only.
     * @return True when document files are in rename-only mode.
     */
    bool get_rename_documents_only() const;
    /**
     * @brief Enables or disables rename-only mode for documents.
     * @param value True to enable rename-only mode for documents.
     */
    void set_rename_documents_only(bool value);
    /**
     * @brief Returns whether only document files are processed.
     * @return True when only document files are processed.
     */
    bool get_process_documents_only() const;
    /**
     * @brief Enables or disables document-only processing.
     * @param value True to enable document-only processing.
     */
    void set_process_documents_only(bool value);
    /**
     * @brief Returns whether to append a document creation date to category names.
     * @return True when document creation dates should be appended to categories.
     */
    bool get_add_document_date_to_category() const;
    /**
     * @brief Enables or disables appending document creation dates to category names.
     * @param value True to append document creation dates to categories.
     */
    void set_add_document_date_to_category(bool value);

    /**
     * @brief Returns the current target sort folder path.
     * @return Sort folder path as UTF-8 text.
     */
    std::string get_sort_folder() const;
    /**
     * @brief Sets the target sort folder path.
     * @param path Sort folder path to store.
     */
    void set_sort_folder(const std::string &path);

    /**
     * @brief Returns whether the consistency-pass feature is enabled.
     * @return True when the consistency pass should be available.
     */
    bool get_consistency_pass_enabled() const;
    /**
     * @brief Enables or disables the consistency-pass feature.
     * @param value True to enable the consistency pass.
     */
    void set_consistency_pass_enabled(bool value);

    /**
     * @brief Returns whether category whitelists are enabled.
     * @return True when whitelist filtering is active.
     */
    bool get_use_whitelist() const;
    /**
     * @brief Enables or disables category whitelist filtering.
     * @param value True to enable whitelist filtering.
     */
    void set_use_whitelist(bool value);
    /**
     * @brief Returns the active category whitelist name.
     * @return Active whitelist name.
     */
    std::string get_active_whitelist() const;
    /**
     * @brief Sets the active category whitelist name.
     * @param name Whitelist name to store.
     */
    void set_active_whitelist(const std::string& name);

    /**
     * @brief Returns whether prompt logging is enabled in development mode.
     * @return True when prompt/response logging is enabled.
     */
    bool get_development_prompt_logging() const;
    /**
     * @brief Enables or disables prompt logging in development mode.
     * @param value True to enable prompt logging.
     */
    void set_development_prompt_logging(bool value);

    /**
     * @brief Resolves the full path to the active `config.ini` file.
     * @return Platform-appropriate config file path.
     */
    std::string define_config_path();
    /**
     * @brief Returns the directory containing the active config file.
     * @return Config directory path as UTF-8 text.
     */
    std::string get_config_dir();

    /**
     * @brief Stores the application version that should be skipped for update prompts.
     * @param version Version string to suppress.
     */
    void set_skipped_version(const std::string &version);
    /**
     * @brief Returns the application version currently skipped for update prompts.
     * @return Skipped version string, or empty when none is set.
     */
    std::string get_skipped_version();
    /**
     * @brief Sets whether the file explorer panel should be shown.
     * @param value True to keep the file explorer visible.
     */
    void set_show_file_explorer(bool value);
    /**
     * @brief Returns whether the file explorer panel should be shown.
     * @return True when the file explorer is visible by default.
     */
    bool get_show_file_explorer() const;
    /**
     * @brief Returns whether the suitability benchmark has been completed.
     * @return True when the benchmark has run at least once.
     */
    bool get_suitability_benchmark_completed() const;
    /**
     * @brief Returns whether the suitability benchmark dialog is suppressed.
     * @return True when the dialog should not auto-show on startup.
     */
    bool get_suitability_benchmark_suppressed() const;
    /**
     * @brief Marks the suitability benchmark as completed.
     * @param value True when the benchmark has run.
     */
    void set_suitability_benchmark_completed(bool value);
    /**
     * @brief Sets whether the suitability benchmark dialog is suppressed.
     * @param value True to suppress auto-showing the dialog.
     */
    void set_suitability_benchmark_suppressed(bool value);
    /**
     * @brief Returns the most recent benchmark report text.
     * @return Report text (may be empty).
     */
    std::string get_benchmark_last_report() const;
    /**
     * @brief Sets the most recent benchmark report text.
     * @param value Report text to store.
     */
    void set_benchmark_last_report(const std::string& value);
    /**
     * @brief Returns the timestamp string for the last benchmark run.
     * @return Timestamp string (may be empty).
     */
    std::string get_benchmark_last_run() const;
    /**
     * @brief Sets the timestamp string for the last benchmark run.
     * @param value Timestamp to store.
     */
    void set_benchmark_last_run(const std::string& value);
    /**
     * @brief Returns the selected UI language.
     * @return Current interface language.
     */
    Language get_language() const;
    /**
     * @brief Sets the selected UI language.
     * @param value Interface language to store.
     */
    void set_language(Language value);
    /**
     * @brief Returns the cumulative number of categorized files recorded so far.
     * @return Total categorized file count.
     */
    int get_total_categorized_files() const;
    /**
     * @brief Adds to the cumulative categorized-file counter.
     * @param count Number of files to add; non-positive values are ignored.
     */
    void add_categorized_files(int count);
    /**
     * @brief Returns the next file-count threshold for the support prompt.
     * @return Next support prompt threshold.
     */
    int get_next_support_prompt_threshold() const;
    /**
     * @brief Sets the next file-count threshold for the support prompt.
     * @param threshold Threshold value to store (clamped to the minimum supported value).
     */
    void set_next_support_prompt_threshold(int threshold);
    /**
     * @brief Returns the configured allowed category whitelist.
     * @return Copy of the allowed category list.
     */
    std::vector<std::string> get_allowed_categories() const;
    /**
     * @brief Replaces the configured allowed category whitelist.
     * @param values Allowed category names to store.
     */
    void set_allowed_categories(std::vector<std::string> values);
    /**
     * @brief Returns the configured allowed subcategory whitelist.
     * @return Copy of the allowed subcategory list.
     */
    std::vector<std::string> get_allowed_subcategories() const;
    /**
     * @brief Replaces the configured allowed subcategory whitelist.
     * @param values Allowed subcategory names to store.
     */
    void set_allowed_subcategories(std::vector<std::string> values);

private:
    LLMChoice parse_llm_choice() const;
    void load_basic_settings(const std::function<bool(const char*, bool)>& load_bool,
                             const std::function<int(const char*, int, int)>& load_int);
    void load_whitelist_settings(const std::function<bool(const char*, bool)>& load_bool);
    void load_custom_llm_settings();
    /**
     * @brief Load custom API endpoint entries from config.
     */
    void load_custom_api_settings();
    void log_loaded_settings() const;

    void save_core_settings();
    void save_whitelist_settings();
    void save_custom_llms();
    /**
     * @brief Save custom API endpoint entries to config.
     */
    void save_custom_api_endpoints();

    std::string config_path;
    std::filesystem::path config_dir;
    IniConfig config;

    LLMChoice llm_choice = LLMChoice::Unset;
    std::string openai_api_key;
    std::string openai_model{ "gpt-4o-mini" };
    std::string gemini_api_key;
    std::string gemini_model{ "gemini-2.5-flash-lite" };
    bool llm_downloads_expanded{true};
    bool use_subcategories;
    bool categorize_files;
    bool categorize_directories;
    bool include_subdirectories{false};
    bool analyze_images_by_content{false};
    bool offer_rename_images{false};
    bool add_image_date_place_to_filename{false};
    bool add_audio_video_metadata_to_filename{true};
    bool add_image_date_to_category{false};
    bool image_options_expanded{false};
    bool rename_images_only{false};
    bool process_images_only{false};
    bool analyze_documents_by_content{false};
    bool offer_rename_documents{false};
    bool document_options_expanded{false};
    bool rename_documents_only{false};
    bool process_documents_only{false};
    bool add_document_date_to_category{false};
    bool use_consistency_hints{false};
    bool use_whitelist{false};
    std::string default_sort_folder;
    std::string sort_folder;
    std::string skipped_version;
    bool show_file_explorer{true};
    bool suitability_benchmark_completed{false};
    bool suitability_benchmark_suppressed{false};
    std::string benchmark_last_report;
    std::string benchmark_last_run;
    Language language{Language::English};
    CategoryLanguage category_language{CategoryLanguage::English};
    bool consistency_pass_enabled{false};
    bool development_prompt_logging{false};
    int categorized_file_count{0};
    int next_support_prompt_threshold{100};
    std::vector<std::string> allowed_categories;
    std::vector<std::string> allowed_subcategories;
    std::string active_whitelist;
    std::vector<CustomLLM> custom_llms;
    std::string active_custom_llm_id;
    std::vector<CustomApiEndpoint> custom_api_endpoints;
    std::string active_custom_api_id;
};

#endif
