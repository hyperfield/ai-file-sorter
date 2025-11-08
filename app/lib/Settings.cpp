#include "Settings.hpp"
#include "Types.hpp"
#include "Logger.hpp"
#include "Language.hpp"
#include "Utils.hpp"
#include <filesystem>
#include <cstdio>
#include <iostream>
#include <QStandardPaths>
#include <QString>
#include <QByteArray>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>
#include <algorithm>
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

int parse_int_or(const std::string& value, int fallback) {
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
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

    auto to_utf8 = [](const QString& value) -> std::string {
        const QByteArray bytes = value.toUtf8();
        return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
    };

    QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (!downloads.isEmpty()) {
        default_sort_folder = to_utf8(downloads);
    } else {
        QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        if (!home.isEmpty()) {
            default_sort_folder = to_utf8(home);
        }
    }

    if (default_sort_folder.empty()) {
        default_sort_folder = Utils::path_to_utf8(std::filesystem::current_path());
    }

    sort_folder = default_sort_folder;
}


std::string Settings::define_config_path()
{
    std::string AppName = "AIFileSorter";
#ifdef _WIN32
    char appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) {
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
        sort_folder = default_sort_folder.empty() ? std::string("/") : default_sort_folder;
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
    sort_folder = config.getValue("Settings", "SortFolder", default_sort_folder.empty() ? std::string("/") : default_sort_folder);
    show_file_explorer = config.getValue("Settings", "ShowFileExplorer", "true") == "true";
    consistency_pass_enabled = config.getValue("Settings", "ConsistencyPass", "false") == "true";
    development_prompt_logging = config.getValue("Settings", "DevelopmentPromptLogging", "false") == "true";
    skipped_version = config.getValue("Settings", "SkippedVersion", "0.0.0");
    const std::string language_value = config.getValue("Settings", "Language", "English");
    language = languageFromString(QString::fromStdString(language_value));
    categorized_file_count = parse_int_or(config.getValue("Settings", "CategorizedFileCount", "0"), 0);
    next_support_prompt_threshold = parse_int_or(config.getValue("Settings", "SupportPromptThreshold", "100"), 100);
    if (next_support_prompt_threshold < 100) {
        next_support_prompt_threshold = 100;
    }

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

    config.setValue("Settings", "ShowFileExplorer", show_file_explorer ? "true" : "false");
    config.setValue("Settings", "ConsistencyPass", consistency_pass_enabled ? "true" : "false");
    config.setValue("Settings", "DevelopmentPromptLogging", development_prompt_logging ? "true" : "false");
    config.setValue("Settings", "Language", languageToString(language).toStdString());
    config.setValue("Settings", "CategorizedFileCount", std::to_string(categorized_file_count));
    config.setValue("Settings", "SupportPromptThreshold", std::to_string(next_support_prompt_threshold));

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

bool Settings::get_consistency_pass_enabled() const
{
    return consistency_pass_enabled;
}

void Settings::set_consistency_pass_enabled(bool value)
{
    consistency_pass_enabled = value;
}

bool Settings::get_development_prompt_logging() const
{
    return development_prompt_logging;
}

void Settings::set_development_prompt_logging(bool value)
{
    development_prompt_logging = value;
}


void Settings::set_skipped_version(const std::string &version) {
    skipped_version = version;
}


std::string Settings::get_skipped_version()
{
    return skipped_version;
}


void Settings::set_show_file_explorer(bool value)
{
    show_file_explorer = value;
}


bool Settings::get_show_file_explorer() const
{
    return show_file_explorer;
}


Language Settings::get_language() const
{
    return language;
}


void Settings::set_language(Language value)
{
    language = value;
}

int Settings::get_total_categorized_files() const
{
    return categorized_file_count;
}

void Settings::add_categorized_files(int count)
{
    if (count <= 0) {
        return;
    }
    categorized_file_count += count;
}

int Settings::get_next_support_prompt_threshold() const
{
    return next_support_prompt_threshold;
}

void Settings::set_next_support_prompt_threshold(int threshold)
{
    if (threshold < 100) {
        threshold = 100;
    }
    next_support_prompt_threshold = threshold;
}
