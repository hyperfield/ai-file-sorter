#include "Settings.hpp"
#include "Types.hpp"
#include "Logger.hpp"
#include "Language.hpp"
#include "Utils.hpp"
#include <filesystem>
#include <cstdio>
#include <iostream>
#include <QStandardPaths>
#include <QLocale>
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

std::string to_bool_string(bool value) {
    return value ? "true" : "false";
}

std::string llm_choice_to_string(LLMChoice choice) {
    switch (choice) {
        case LLMChoice::Local_3b: return "Local_3b";
        case LLMChoice::Local_7b: return "Local_7b";
        case LLMChoice::Remote: return "Remote";
        case LLMChoice::Gemini: return "Gemini";
        case LLMChoice::Custom: return "Custom";
        default: return "Unset";
    }
}

std::string trim_string(const std::string& str) {
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    auto trimmed = str;
    trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), not_space));
    trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), not_space).base(), trimmed.end());
    return trimmed;
}

void set_bool_setting(IniConfig& config, const std::string& section, const char* key, bool value) {
    config.setValue(section, key, to_bool_string(value));
}

void set_optional_setting(IniConfig& config, const std::string& section, const char* key, const std::string& value) {
    if (!value.empty()) {
        config.setValue(section, key, value);
    }
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

Language system_default_language()
{
    switch (QLocale::system().language()) {
        case QLocale::French: return Language::French;
        case QLocale::German: return Language::German;
        case QLocale::Italian: return Language::Italian;
        case QLocale::Spanish: return Language::Spanish;
        case QLocale::Turkish: return Language::Turkish;
        default: return Language::English;
    }
}
}


Settings::Settings()
    : use_subcategories(true),
      categorize_files(true),
      categorize_directories(false),
      use_consistency_hints(false),
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

    // Default language follows system locale on first run (before any config file exists).
    language = system_default_language();
    category_language = CategoryLanguage::English;
}

LLMChoice Settings::parse_llm_choice() const
{
    const std::string value = config.getValue("Settings", "LLMChoice", "Unset");
    if (value == "Local_3b") return LLMChoice::Local_3b;
    if (value == "Local_7b") return LLMChoice::Local_7b;
    if (value == "Remote")   return LLMChoice::Remote;
    if (value == "Gemini")   return LLMChoice::Gemini;
    if (value == "Custom")   return LLMChoice::Custom;
    return LLMChoice::Unset;
}

void Settings::load_basic_settings(const std::function<bool(const char*, bool)>& load_bool,
                                   const std::function<int(const char*, int, int)>& load_int)
{
    llm_choice = parse_llm_choice();
    set_remote_api_key(config.getValue("Settings", "RemoteApiKey", ""));
    set_remote_model(config.getValue("Settings", "RemoteModel", "gpt-4o-mini"));
    set_gemini_api_key(config.getValue("Settings", "GeminiApiKey", ""));
    set_gemini_model(config.getValue("Settings", "GeminiModel", "gemini-1.5-flash"));
    use_subcategories = load_bool("UseSubcategories", false);
    use_consistency_hints = load_bool("UseConsistencyHints", false);
    categorize_files = load_bool("CategorizeFiles", true);
    categorize_directories = load_bool("CategorizeDirectories", false);
    sort_folder = config.getValue("Settings", "SortFolder", default_sort_folder.empty() ? std::string("/") : default_sort_folder);
    show_file_explorer = load_bool("ShowFileExplorer", true);
    consistency_pass_enabled = load_bool("ConsistencyPass", false);
    development_prompt_logging = load_bool("DevelopmentPromptLogging", false);
    skipped_version = config.getValue("Settings", "SkippedVersion", "0.0.0");
    if (config.hasValue("Settings", "Language")) {
        language = languageFromString(QString::fromStdString(config.getValue("Settings", "Language", "English")));
    } else {
        language = system_default_language();
    }
    category_language = categoryLanguageFromString(QString::fromStdString(config.getValue("Settings", "CategoryLanguage", "English")));
    categorized_file_count = load_int("CategorizedFileCount", 0, 0);
    next_support_prompt_threshold = load_int("SupportPromptThreshold", 200, 200);
}

void Settings::load_whitelist_settings(const std::function<bool(const char*, bool)>& load_bool)
{
    allowed_categories = parse_list(config.getValue("Settings", "AllowedCategories", ""));
    allowed_subcategories = parse_list(config.getValue("Settings", "AllowedSubcategories", ""));
    use_whitelist = load_bool("UseWhitelist", false);
    active_whitelist = config.getValue("Settings", "ActiveWhitelist", "");
}

void Settings::load_custom_llm_settings()
{
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
}

void Settings::log_loaded_settings() const
{
    if (auto logger = Logger::get_logger("core_logger")) {
        logger->info("Loaded settings from '{}' (allowed categories: {}, allowed subcategories: {}, use whitelist: {}, active whitelist: '{}', custom llms: {}, category language: {})",
                     config_path,
                     allowed_categories.size(),
                     allowed_subcategories.size(),
                     use_whitelist,
                     active_whitelist,
                     custom_llms.size(),
                     categoryLanguageDisplay(category_language));
    }
}

void Settings::save_core_settings()
{
    static const std::string settings_section = "Settings";

    config.setValue(settings_section, "LLMChoice", llm_choice_to_string(llm_choice));
    config.setValue(settings_section, "RemoteApiKey", remote_api_key);
    config.setValue(settings_section, "RemoteModel", remote_model.empty() ? "gpt-4o-mini" : remote_model);
    config.setValue(settings_section, "GeminiApiKey", gemini_api_key);
    config.setValue(settings_section, "GeminiModel", gemini_model.empty() ? "gemini-1.5-flash" : gemini_model);
    set_bool_setting(config, settings_section, "UseSubcategories", use_subcategories);
    set_bool_setting(config, settings_section, "UseConsistencyHints", use_consistency_hints);
    set_bool_setting(config, settings_section, "CategorizeFiles", categorize_files);
    set_bool_setting(config, settings_section, "CategorizeDirectories", categorize_directories);
    config.setValue(settings_section, "SortFolder", this->sort_folder);

    set_optional_setting(config, settings_section, "SkippedVersion", skipped_version);

    set_bool_setting(config, settings_section, "ShowFileExplorer", show_file_explorer);
    set_bool_setting(config, settings_section, "ConsistencyPass", consistency_pass_enabled);
    set_bool_setting(config, settings_section, "DevelopmentPromptLogging", development_prompt_logging);
    config.setValue(settings_section, "Language", languageToString(language).toStdString());
    config.setValue(settings_section, "CategoryLanguage", categoryLanguageToString(category_language).toStdString());
    config.setValue(settings_section, "CategorizedFileCount", std::to_string(categorized_file_count));
    config.setValue(settings_section, "SupportPromptThreshold", std::to_string(next_support_prompt_threshold));
}

void Settings::save_whitelist_settings()
{
    static const std::string settings_section = "Settings";

    config.setValue(settings_section, "AllowedCategories", join_list(allowed_categories));
    config.setValue(settings_section, "AllowedSubcategories", join_list(allowed_subcategories));
    set_bool_setting(config, settings_section, "UseWhitelist", use_whitelist);
    set_optional_setting(config, settings_section, "ActiveWhitelist", active_whitelist);
}

void Settings::save_custom_llms()
{
    static const std::string llm_section = "LLMs";

    set_optional_setting(config, llm_section, "ActiveCustomId", active_custom_llm_id);

    std::vector<std::string> ids;
    ids.reserve(custom_llms.size());
    for (const auto& entry : custom_llms) {
        if (!is_valid_custom_llm(entry)) {
            continue;
        }
        ids.push_back(entry.id);
        const std::string section = "LLM_" + entry.id;
        config.setValue(section, "Name", entry.name);
        config.setValue(section, "Description", entry.description);
        config.setValue(section, "Path", entry.path);
    }
    config.setValue(llm_section, "CustomIds", join_list(ids));
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
        // Keep language defaults derived from system locale when no config is found.
        return false;
    }

    const auto load_bool = [&](const char* key, bool def) {
        return config.getValue("Settings", key, def ? "true" : "false") == "true";
    };

    const auto load_int = [&](const char* key, int def, int min_val = std::numeric_limits<int>::min()) {
        int value = parse_int_or(config.getValue("Settings", key, std::to_string(def)), def);
        return value < min_val ? min_val : value;
    };

    load_basic_settings(load_bool, load_int);
    load_whitelist_settings(load_bool);
    load_custom_llm_settings();
    log_loaded_settings();

    return true;
}


bool Settings::save()
{
    save_core_settings();
    save_whitelist_settings();
    save_custom_llms();
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

std::string Settings::get_remote_api_key() const
{
    return remote_api_key;
}

void Settings::set_remote_api_key(const std::string& key)
{
    remote_api_key = trim_string(key);
}

std::string Settings::get_remote_model() const
{
    return remote_model;
}

void Settings::set_remote_model(const std::string& model)
{
    auto trimmed = trim_string(model);
    if (trimmed.empty()) {
        trimmed = "gpt-4o-mini";
    }
    remote_model = trimmed;
}

std::string Settings::get_gemini_api_key() const
{
    return gemini_api_key;
}

void Settings::set_gemini_api_key(const std::string& key)
{
    gemini_api_key = trim_string(key);
}

std::string Settings::get_gemini_model() const
{
    return gemini_model;
}

void Settings::set_gemini_model(const std::string& model)
{
    auto trimmed = trim_string(model);
    if (trimmed.empty()) {
        trimmed = "gemini-1.5-flash";
    }
    gemini_model = trimmed;
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

CategoryLanguage Settings::get_category_language() const
{
    return category_language;
}

void Settings::set_category_language(CategoryLanguage language)
{
    category_language = language;
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
