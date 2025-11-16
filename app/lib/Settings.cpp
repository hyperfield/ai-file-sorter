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
#include <sstream>
#include <cctype>
#include <chrono>
#include <random>
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

std::vector<std::string> parse_list(const std::string& value) {
    std::vector<std::string> result;
    std::stringstream ss(value);
    std::string item;
    while (std::getline(ss, item, ',')) {
        auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
        item.erase(item.begin(), std::find_if(item.begin(), item.end(), not_space));
        item.erase(std::find_if(item.rbegin(), item.rend(), not_space).base(), item.end());
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    return result;
}

std::string join_list(const std::vector<std::string>& items) {
    std::ostringstream oss;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << items[i];
    }
    return oss.str();
}

std::string generate_custom_llm_id() {
    using clock = std::chrono::steady_clock;
    const auto now = clock::now().time_since_epoch().count();
    std::mt19937_64 rng(static_cast<std::mt19937_64::result_type>(now));
    const uint64_t value = rng();
    std::ostringstream oss;
    oss << "llm_" << std::hex << value;
    return oss.str();
}
}


Settings::Settings()
    : use_subcategories(true),
      categorize_files(true),
      categorize_directories(false),
      use_consistency_hints(true),
      use_whitelist(false),
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
    if (const char* override_root = std::getenv("AI_FILE_SORTER_CONFIG_DIR")) {
        std::filesystem::path base = override_root;
        return (base / AppName / "config.ini").string();
    }
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
    else if (load_choice_value == "Custom") llm_choice = LLMChoice::Custom;
    else llm_choice = LLMChoice::Unset;

    use_subcategories = config.getValue("Settings", "UseSubcategories", "false") == "true";
    use_consistency_hints = config.getValue("Settings", "UseConsistencyHints", "true") == "true";
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

    allowed_categories = parse_list(config.getValue("Settings", "AllowedCategories", ""));
    allowed_subcategories = parse_list(config.getValue("Settings", "AllowedSubcategories", ""));
    use_whitelist = config.getValue("Settings", "UseWhitelist", "false") == "true";
    active_whitelist = config.getValue("Settings", "ActiveWhitelist", "");
    active_custom_llm_id = config.getValue("LLMs", "ActiveCustomId", "");

    custom_llms.clear();
    const auto custom_ids = parse_list(config.getValue("LLMs", "CustomIds", ""));
    for (const auto& id : custom_ids) {
        const std::string section = "LLM_" + id;
        CustomLLM entry;
        entry.id = id;
        entry.name = config.getValue(section, "Name", "");
        entry.description = config.getValue(section, "Description", "");
        entry.path = config.getValue(section, "Path", "");
        if (!entry.name.empty() && !entry.path.empty()) {
            custom_llms.push_back(entry);
        }
    }

    if (auto logger = Logger::get_logger("core_logger")) {
        logger->info("Loaded settings from '{}' (allowed categories: {}, allowed subcategories: {}, use whitelist: {}, active whitelist: '{}', custom llms: {})",
                     config_path,
                     allowed_categories.size(),
                     allowed_subcategories.size(),
                     use_whitelist,
                     active_whitelist,
                     custom_llms.size());
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
        case LLMChoice::Custom: save_choice_value = "Custom"; break;
        default: save_choice_value = "Unset"; break;
    }
    config.setValue("Settings", "LLMChoice", save_choice_value);


    config.setValue("Settings", "UseSubcategories", use_subcategories ? "true" : "false");
    config.setValue("Settings", "UseConsistencyHints", use_consistency_hints ? "true" : "false");
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

    config.setValue("Settings", "AllowedCategories", join_list(allowed_categories));
    config.setValue("Settings", "AllowedSubcategories", join_list(allowed_subcategories));
    config.setValue("Settings", "UseWhitelist", use_whitelist ? "true" : "false");
    if (!active_whitelist.empty()) {
        config.setValue("Settings", "ActiveWhitelist", active_whitelist);
    }
    if (!active_custom_llm_id.empty()) {
        config.setValue("LLMs", "ActiveCustomId", active_custom_llm_id);
    }

    std::vector<std::string> ids;
    ids.reserve(custom_llms.size());
    for (const auto& entry : custom_llms) {
        if (entry.id.empty() || entry.name.empty() || entry.path.empty()) {
            continue;
        }
        ids.push_back(entry.id);
        const std::string section = "LLM_" + entry.id;
        config.setValue(section, "Name", entry.name);
        config.setValue(section, "Description", entry.description);
        config.setValue(section, "Path", entry.path);
    }
    config.setValue("LLMs", "CustomIds", join_list(ids));

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

std::string Settings::get_active_custom_llm_id() const
{
    return active_custom_llm_id;
}

void Settings::set_active_custom_llm_id(const std::string& id)
{
    active_custom_llm_id = id;
}

const std::vector<CustomLLM>& Settings::get_custom_llms() const
{
    return custom_llms;
}

CustomLLM Settings::find_custom_llm(const std::string& id) const
{
    const auto it = std::find_if(custom_llms.begin(), custom_llms.end(),
                                 [&id](const CustomLLM& item) { return item.id == id; });
    if (it != custom_llms.end()) {
        return *it;
    }
    return {};
}

std::string Settings::upsert_custom_llm(const CustomLLM& llm)
{
    CustomLLM copy = llm;
    if (copy.id.empty()) {
        copy.id = generate_custom_llm_id();
    }
    const auto it = std::find_if(custom_llms.begin(), custom_llms.end(),
                                 [&copy](const CustomLLM& item) { return item.id == copy.id; });
    if (it != custom_llms.end()) {
        *it = copy;
    } else {
        custom_llms.push_back(copy);
    }
    return copy.id;
}

void Settings::remove_custom_llm(const std::string& id)
{
    custom_llms.erase(std::remove_if(custom_llms.begin(),
                                     custom_llms.end(),
                                     [&id](const CustomLLM& item) { return item.id == id; }),
                      custom_llms.end());
    if (active_custom_llm_id == id) {
        active_custom_llm_id.clear();
    }
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

bool Settings::get_use_consistency_hints() const
{
    return use_consistency_hints;
}

void Settings::set_use_consistency_hints(bool value)
{
    use_consistency_hints = value;
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

bool Settings::get_use_whitelist() const
{
    return use_whitelist;
}

void Settings::set_use_whitelist(bool value)
{
    use_whitelist = value;
}

std::string Settings::get_active_whitelist() const
{
    return active_whitelist;
}

void Settings::set_active_whitelist(const std::string& name)
{
    active_whitelist = name;
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

std::vector<std::string> Settings::get_allowed_categories() const
{
    return allowed_categories;
}

void Settings::set_allowed_categories(std::vector<std::string> values)
{
    allowed_categories = std::move(values);
}

std::vector<std::string> Settings::get_allowed_subcategories() const
{
    return allowed_subcategories;
}

void Settings::set_allowed_subcategories(std::vector<std::string> values)
{
    allowed_subcategories = std::move(values);
}
