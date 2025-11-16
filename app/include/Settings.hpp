#ifndef SETTINGS_HPP
#define SETTINGS_HPP

#include <IniConfig.hpp>
#include <Types.hpp>
#include <Language.hpp>
#include <string>
#include <filesystem>
#include <vector>


class Settings
{
public:
    Settings();

    bool load();
    bool save();

    LLMChoice get_llm_choice() const;
    void set_llm_choice(LLMChoice choice);
    bool is_llm_chosen() const;

    bool get_use_subcategories() const;
    void set_use_subcategories(bool value);

    bool get_use_consistency_hints() const;
    void set_use_consistency_hints(bool value);

    bool get_categorize_files() const;
    void set_categorize_files(bool value);

    bool get_categorize_directories() const;
    void set_categorize_directories(bool value);

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
    std::string config_path;
    std::filesystem::path config_dir;
    IniConfig config;

    LLMChoice llm_choice = LLMChoice::Unset;
    bool use_subcategories;
    bool categorize_files;
    bool categorize_directories;
    bool use_consistency_hints{true};
    bool use_whitelist{false};
    std::string default_sort_folder;
    std::string sort_folder;
    std::string skipped_version;
    bool show_file_explorer{true};
    Language language{Language::English};
    bool consistency_pass_enabled{false};
    bool development_prompt_logging{false};
    int categorized_file_count{0};
    int next_support_prompt_threshold{100};
    std::vector<std::string> allowed_categories;
    std::vector<std::string> allowed_subcategories;
    std::string active_whitelist;
};

#endif
