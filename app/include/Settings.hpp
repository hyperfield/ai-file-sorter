#ifndef SETTINGS_HPP
#define SETTINGS_HPP

#include <IniConfig.hpp>
#include <Types.hpp>
#include <Language.hpp>
#include <string>
#include <filesystem>


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

    bool get_categorize_files() const;
    void set_categorize_files(bool value);

    bool get_categorize_directories() const;
    void set_categorize_directories(bool value);

    std::string get_sort_folder() const;
    void set_sort_folder(const std::string &path);

    std::string define_config_path();
    std::string get_config_dir();

    void set_skipped_version(const std::string &version);
    std::string get_skipped_version();
    void set_show_file_explorer(bool value);
    bool get_show_file_explorer() const;
    Language get_language() const;
    void set_language(Language value);

private:
    std::string config_path;
    std::filesystem::path config_dir;
    IniConfig config;

    LLMChoice llm_choice = LLMChoice::Unset;
    bool use_subcategories;
    bool categorize_files;
    bool categorize_directories;
    std::string default_sort_folder;
    std::string sort_folder;
    std::string skipped_version;
    bool show_file_explorer{true};
    Language language{Language::English};
};

#endif
