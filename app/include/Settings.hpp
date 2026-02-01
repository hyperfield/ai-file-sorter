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


class Settings
{
public:
    Settings();

    bool load();
    bool save();

    LLMChoice get_llm_choice() const;
    void set_llm_choice(LLMChoice choice);
    std::string get_openai_api_key() const;
    void set_openai_api_key(const std::string& key);
    std::string get_openai_model() const;
    void set_openai_model(const std::string& model);
    std::string get_gemini_api_key() const;
    void set_gemini_api_key(const std::string& key);
    std::string get_gemini_model() const;
    void set_gemini_model(const std::string& model);
    CategoryLanguage get_category_language() const;
    void set_category_language(CategoryLanguage language);
    std::string get_active_custom_llm_id() const;
    void set_active_custom_llm_id(const std::string& id);
    const std::vector<CustomLLM>& get_custom_llms() const;
    std::string upsert_custom_llm(const CustomLLM& llm);
    void remove_custom_llm(const std::string& id);
    CustomLLM find_custom_llm(const std::string& id) const;
    /**
     * @brief Return the active custom API endpoint id.
     */
    std::string get_active_custom_api_id() const;
    /**
     * @brief Set the active custom API endpoint id.
     */
    void set_active_custom_api_id(const std::string& id);
    /**
     * @brief Return the configured custom API endpoints.
     */
    const std::vector<CustomApiEndpoint>& get_custom_api_endpoints() const;
    /**
     * @brief Add or update a custom API endpoint entry.
     */
    std::string upsert_custom_api_endpoint(const CustomApiEndpoint& endpoint);
    /**
     * @brief Remove a custom API endpoint by id.
     */
    void remove_custom_api_endpoint(const std::string& id);
    /**
     * @brief Find a custom API endpoint by id.
     */
    CustomApiEndpoint find_custom_api_endpoint(const std::string& id) const;
    bool is_llm_chosen() const;

    bool get_use_subcategories() const;
    void set_use_subcategories(bool value);

    bool get_use_consistency_hints() const;
    void set_use_consistency_hints(bool value);

    bool get_categorize_files() const;
    void set_categorize_files(bool value);

    bool get_categorize_directories() const;
    void set_categorize_directories(bool value);

    bool get_include_subdirectories() const;
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

    std::string get_sort_folder() const;
    void set_sort_folder(const std::string &path);

    bool get_consistency_pass_enabled() const;
    void set_consistency_pass_enabled(bool value);

    bool get_use_whitelist() const;
    void set_use_whitelist(bool value);
    std::string get_active_whitelist() const;
    void set_active_whitelist(const std::string& name);

    bool get_development_prompt_logging() const;
    void set_development_prompt_logging(bool value);

    std::string define_config_path();
    std::string get_config_dir();

    void set_skipped_version(const std::string &version);
    std::string get_skipped_version();
    void set_show_file_explorer(bool value);
    bool get_show_file_explorer() const;
    Language get_language() const;
    void set_language(Language value);
    int get_total_categorized_files() const;
    void add_categorized_files(int count);
    int get_next_support_prompt_threshold() const;
    void set_next_support_prompt_threshold(int threshold);
    std::vector<std::string> get_allowed_categories() const;
    void set_allowed_categories(std::vector<std::string> values);
    std::vector<std::string> get_allowed_subcategories() const;
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

    LLMChoice llm_choice = LLMChoice::Local_7b;
    std::string openai_api_key;
    std::string openai_model{ "gpt-4o-mini" };
    std::string gemini_api_key;
    std::string gemini_model{ "gemini-2.5-flash-lite" };
    bool use_subcategories;
    bool categorize_files;
    bool categorize_directories;
    bool include_subdirectories{false};
    bool analyze_images_by_content{false};
    bool offer_rename_images{false};
    bool rename_images_only{false};
    bool process_images_only{false};
    bool analyze_documents_by_content{false};
    bool offer_rename_documents{false};
    bool rename_documents_only{false};
    bool process_documents_only{false};
    bool add_document_date_to_category{false};
    bool use_consistency_hints{false};
    bool use_whitelist{false};
    std::string default_sort_folder;
    std::string sort_folder;
    std::string skipped_version;
    bool show_file_explorer{true};
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
