#include "Settings.hpp"
#include "Types.hpp"
#include "Logger.hpp"
#include <filesystem>
#include <cstdio>
#include <iostream>
#include <glib.h>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>
#ifdef _WIN32
    #include <shlobj.h>
    #include <windows.h>
#endif


namespace {
template <typename... Args>
void settings_log(spdlog::level::level_enum level, const char* fmt, Args&&... args) {
    auto message = fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...);
    if (auto logger = Logger::get_logger("core_logger")) {
        logger->log(level, "{}", message);
    } else {
        std::fprintf(stderr, "%s\n", message.c_str());
    }
}
}


Settings::Settings()
    : use_subcategories(true),
      categorize_files(true),
      categorize_directories(false),
      default_sort_folder(""),
      sort_folder("")
{
    std::string AppName = "AIFileSorter";
    config_path = define_config_path();

    config_dir = std::filesystem::path(config_path).parent_path();

    try {
        if (!std::filesystem::exists(config_dir)) {
            std::filesystem::create_directories(config_dir);
        }
    } catch (const std::filesystem::filesystem_error &e) {
        settings_log(spdlog::level::err, "Error creating configuration directory: {}", e.what());
    }
    this->default_sort_folder = g_get_user_special_dir(G_USER_DIRECTORY_DOWNLOAD);
    if (!this->default_sort_folder) {
        this->default_sort_folder = g_get_home_dir();
    }
    this->sort_folder = this->default_sort_folder;
}


std::string Settings::define_config_path()
{
    std::string AppName = "AIFileSorter";
#ifdef _WIN32
    char appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) {
        return std::string(appDataPath) + "\\" + AppName + "\\config.ini";
    }
#elif defined(__APPLE__)
    return std::string(getenv("HOME")) + "/Library/Application Support/" + AppName + "/config.ini";
#else
    return std::string(getenv("HOME")) + "/.config/" + AppName + "/config.ini";
#endif
    return "config.ini";
}


std::string Settings::get_config_dir()
{
    return config_dir.string();
}


bool Settings::load()
{
    if (!config.load(config_path)) {
        sort_folder = default_sort_folder ? default_sort_folder : "/";
        return false;
    }

    std::string load_choice_value = config.getValue("Settings", "LLMChoice", "Unset");
    if (load_choice_value == "Local_3b") llm_choice = LLMChoice::Local_3b;
    else if (load_choice_value == "Local_7b") llm_choice = LLMChoice::Local_7b;
    else if (load_choice_value == "Remote") llm_choice = LLMChoice::Remote;
    else llm_choice = LLMChoice::Unset;

    use_subcategories = config.getValue("Settings", "UseSubcategories", "false") == "true";
    categorize_files = config.getValue("Settings", "CategorizeFiles", "true") == "true";
    categorize_directories = config.getValue("Settings", "CategorizeDirectories", "false") == "true";
    sort_folder = config.getValue("Settings", "SortFolder", default_sort_folder ? default_sort_folder : "/");
    skipped_version = config.getValue("Settings", "SkippedVersion", "0.0.0");

    return true;
}


bool Settings::save()
{
    std::string save_choice_value;
    switch (llm_choice) {
        case LLMChoice::Local_3b: save_choice_value = "Local_3b"; break;
        case LLMChoice::Local_7b: save_choice_value = "Local_7b"; break;
        case LLMChoice::Remote: save_choice_value = "Remote"; break;
        default: save_choice_value = "Unset"; break;
    }
    config.setValue("Settings", "LLMChoice", save_choice_value);


    config.setValue("Settings", "UseSubcategories", use_subcategories ? "true" : "false");
    config.setValue("Settings", "CategorizeFiles", categorize_files ? "true" : "false");
    config.setValue("Settings", "CategorizeDirectories", categorize_directories ? "true" : "false");
    config.setValue("Settings", "SortFolder", this->sort_folder);

    if (!skipped_version.empty()) {
        config.setValue("Settings", "SkippedVersion", skipped_version);
    }

    return config.save(config_path);
}


LLMChoice Settings::get_llm_choice() const
{
    return llm_choice;
}


void Settings::set_llm_choice(LLMChoice choice)
{
    llm_choice = choice;
}


bool Settings::is_llm_chosen() const {
    return llm_choice != LLMChoice::Unset;
}


bool Settings::get_use_subcategories() const
{
    return use_subcategories;
}


void Settings::set_use_subcategories(bool value)
{
    use_subcategories = value;
}


bool Settings::get_categorize_files() const
{
    return categorize_files;
}


void Settings::set_categorize_files(bool value)
{
    categorize_files = value;
}


bool Settings::get_categorize_directories() const
{
    return categorize_directories;
}


void Settings::set_categorize_directories(bool value)
{
    categorize_directories = value;
}


std::string Settings::get_sort_folder() const
{
    return sort_folder;
}


void Settings::set_sort_folder(const std::string &path)
{
    this->sort_folder = path;
}


void Settings::set_skipped_version(const std::string &version) {
    skipped_version = version;
}


std::string Settings::get_skipped_version()
{
    return skipped_version;
}
