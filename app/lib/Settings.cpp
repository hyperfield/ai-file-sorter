#include "Settings.hpp"
#include "Types.hpp"
#include "Logger.hpp"
#include "Language.hpp"
#include "Utils.hpp"
#include "VisualModelCatalog.hpp"
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

std::string trim_copy(const std::string& value) {
    auto trimmed = value;
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), not_space));
    trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), not_space).base(), trimmed.end());
    return trimmed;
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

std::string escape_map_token(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for (char ch : value) {
        if (ch == '\\' || ch == ';' || ch == '=' || ch == '|') {
            escaped.push_back('\\');
        }
        escaped.push_back(ch);
    }
    return escaped;
}

std::string unescape_map_token(const std::string& value)
{
    std::string unescaped;
    unescaped.reserve(value.size());
    bool escaped = false;
    for (char ch : value) {
        if (escaped) {
            unescaped.push_back(ch);
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        unescaped.push_back(ch);
    }
    if (escaped) {
        unescaped.push_back('\\');
    }
    return unescaped;
}

std::vector<std::string> split_escaped(const std::string& value, char delimiter)
{
    std::vector<std::string> segments;
    std::string current;
    current.reserve(value.size());
    bool escaped = false;
    for (char ch : value) {
        if (escaped) {
            current.push_back('\\');
            current.push_back(ch);
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == delimiter) {
            segments.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    if (escaped) {
        current.push_back('\\');
    }
    segments.push_back(current);
    return segments;
}

std::size_t find_unescaped(const std::string& value, char target)
{
    bool escaped = false;
    for (std::size_t i = 0; i < value.size(); ++i) {
        const char ch = value[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == target) {
            return i;
        }
    }
    return std::string::npos;
}

std::unordered_map<std::string, std::vector<std::string>> parse_subcategory_map(
    const std::string& value)
{
    std::unordered_map<std::string, std::vector<std::string>> result;
    for (const auto& entry : split_escaped(value, ';')) {
        const auto delimiter = find_unescaped(entry, '=');
        if (delimiter == std::string::npos) {
            continue;
        }
        const std::string category = trim_copy(unescape_map_token(entry.substr(0, delimiter)));
        if (category.empty()) {
            continue;
        }

        std::vector<std::string> subcategories;
        for (const auto& raw_subcategory : split_escaped(entry.substr(delimiter + 1), '|')) {
            const std::string subcategory = trim_copy(unescape_map_token(raw_subcategory));
            if (!subcategory.empty() &&
                std::find(subcategories.begin(), subcategories.end(), subcategory) == subcategories.end()) {
                subcategories.push_back(subcategory);
            }
        }
        if (!subcategories.empty()) {
            result[category] = std::move(subcategories);
        }
    }
    return result;
}

std::string join_subcategory_map(
    const std::unordered_map<std::string, std::vector<std::string>>& values)
{
    std::vector<std::string> categories;
    categories.reserve(values.size());
    for (const auto& [category, subcategories] : values) {
        if (!category.empty() && !subcategories.empty()) {
            categories.push_back(category);
        }
    }
    std::sort(categories.begin(), categories.end());

    std::ostringstream oss;
    bool first_entry = true;
    for (const auto& category : categories) {
        const auto it = values.find(category);
        if (it == values.end() || it->second.empty()) {
            continue;
        }
        if (!first_entry) {
            oss << ";";
        }
        first_entry = false;
        oss << escape_map_token(category) << "=";
        for (std::size_t i = 0; i < it->second.size(); ++i) {
            if (i > 0) {
                oss << "|";
            }
            oss << escape_map_token(it->second[i]);
        }
    }
    return oss.str();
}

std::string to_bool_string(bool value) {
    return value ? "true" : "false";
}

std::string encode_multiline(const std::string& value) {
    std::string output;
    output.reserve(value.size());
    for (char ch : value) {
        if (ch == '\\') {
            output.append("\\\\");
        } else if (ch == '\n') {
            output.append("\\n");
        } else {
            output.push_back(ch);
        }
    }
    return output;
}

std::string decode_multiline(const std::string& value) {
    std::string output;
    output.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        char ch = value[i];
        if (ch == '\\' && i + 1 < value.size()) {
            char next = value[i + 1];
            if (next == 'n') {
                output.push_back('\n');
                ++i;
                continue;
            }
            if (next == '\\') {
                output.push_back('\\');
                ++i;
                continue;
            }
        }
        output.push_back(ch);
    }
    return output;
}

std::vector<std::string> parse_multiline_list(const std::string& value)
{
    std::vector<std::string> items;
    const std::string decoded = decode_multiline(value);
    std::stringstream ss(decoded);
    std::string item;
    while (std::getline(ss, item)) {
        item = trim_copy(item);
        if (!item.empty()) {
            items.push_back(item);
        }
    }
    return items;
}

std::string join_multiline_list(const std::vector<std::string>& items)
{
    std::ostringstream oss;
    bool first = true;
    for (const auto& item : items) {
        const std::string trimmed = trim_copy(item);
        if (trimmed.empty()) {
            continue;
        }
        if (!first) {
            oss << '\n';
        }
        first = false;
        oss << trimmed;
    }
    return encode_multiline(oss.str());
}

std::string llm_choice_to_string(LLMChoice choice) {
    switch (choice) {
        case LLMChoice::Remote_OpenAI: return "Remote_OpenAI";
        case LLMChoice::Remote_Gemini: return "Remote_Gemini";
        case LLMChoice::Remote_Custom: return "Remote_Custom";
        case LLMChoice::Local_4b_Gemma: return "Local_4b_Gemma";
        case LLMChoice::Local_3b_legacy: return "Local_3b_legacy";
        case LLMChoice::Local_7b: return "Local_7b";
        case LLMChoice::Local_7b_Gemma: return "Local_7b_Gemma";
        case LLMChoice::Custom: return "Custom";
        default: return "Unset";
    }
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

std::string generate_custom_api_id() {
    using clock = std::chrono::steady_clock;
    const auto now = clock::now().time_since_epoch().count();
    std::mt19937_64 rng(static_cast<std::mt19937_64::result_type>(now));
    const uint64_t value = rng();
    std::ostringstream oss;
    oss << "api_" << std::hex << value;
    return oss.str();
}

std::string normalize_visual_model_id(const std::string& value)
{
    const auto trimmed = trim_copy(value);
    if (trimmed.empty()) {
        return default_visual_model_descriptor().id;
    }
    if (is_custom_visual_model_id(trimmed)) {
        return trimmed;
    }
    if (find_visual_model_descriptor(trimmed)) {
        return trimmed;
    }
    return default_visual_model_descriptor().id;
}

Language system_default_language()
{
    const QLocale locale = QLocale::system();
    switch (locale.language()) {
        case QLocale::Chinese:
            if (locale.script() == QLocale::SimplifiedHanScript
                || locale.territory() == QLocale::China
                || locale.territory() == QLocale::Singapore) {
                return Language::SimplifiedChinese;
            }
            return Language::English;
        case QLocale::Danish: return Language::Danish;
        case QLocale::Finnish: return Language::Finnish;
        case QLocale::French: return Language::French;
        case QLocale::German: return Language::German;
        case QLocale::Hindi: return Language::Hindi;
        case QLocale::Icelandic: return Language::Icelandic;
        case QLocale::Italian: return Language::Italian;
        case QLocale::NorwegianBokmal: return Language::Norwegian;
        case QLocale::NorwegianNynorsk: return Language::Norwegian;
        case QLocale::Swedish: return Language::Swedish;
        case QLocale::Spanish: return Language::Spanish;
        case QLocale::Turkish: return Language::Turkish;
        case QLocale::Korean: return Language::Korean;
        case QLocale::Dutch: return Language::Dutch;
        default: return Language::English;
    }
}

std::string path_from_env_url(const char* env_key)
{
    const char* url = std::getenv(env_key);
    if (!url || *url == '\0') {
        return {};
    }
    try {
        return Utils::make_default_path_to_file_from_download_url(url);
    } catch (...) {
        return {};
    }
}

bool file_exists_for_env_url(const char* env_key)
{
    const std::string path = path_from_env_url(env_key);
    if (path.empty()) {
        return false;
    }
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}
}


Settings::Settings()
    : use_subcategories(true),
      categorize_files(true),
      categorize_directories(false),
      include_subdirectories(false),
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
    visual_model_id = default_visual_model_descriptor().id;
    analyze_images_by_content = false;
    offer_rename_images = false;
    add_image_date_place_to_filename = false;
    add_audio_video_metadata_to_filename = true;
    add_image_date_to_category = false;
    analyze_documents_by_content = false;
    offer_rename_documents = false;
    rename_documents_only = false;
    process_documents_only = false;
    add_document_date_to_category = false;
}

LLMChoice Settings::parse_llm_choice() const
{
    const std::string value = config.getValue("Settings", "LLMChoice", "Unset");
    if (value == "Remote" || value == "Remote_OpenAI") return LLMChoice::Remote_OpenAI;
    if (value == "Remote_Gemini") return LLMChoice::Remote_Gemini;
    if (value == "Remote_Custom") return LLMChoice::Remote_Custom;
    if (value == "Local_3b" || value == "Local_4b_Gemma") return LLMChoice::Local_4b_Gemma;
    if (value == "Local_3b_legacy") return LLMChoice::Local_3b_legacy;
    if (value == "Local_7b") return LLMChoice::Local_7b;
    if (value == "Local_7b_Gemma") return LLMChoice::Local_7b_Gemma;
    if (value == "Custom")   return LLMChoice::Custom;
    return LLMChoice::Unset;
}

void Settings::load_basic_settings(const std::function<bool(const char*, bool)>& load_bool,
                                   const std::function<int(const char*, int, int)>& load_int)
{
    llm_choice = parse_llm_choice();
    set_openai_api_key(config.getValue("Settings", "RemoteApiKey", ""));
    set_openai_model(config.getValue("Settings", "RemoteModel", "gpt-4o-mini"));
    set_gemini_api_key(config.getValue("Settings", "GeminiApiKey", ""));
    set_gemini_model(config.getValue("Settings", "GeminiModel", "gemini-2.5-flash-lite"));
    set_remote_requests_per_minute(load_int("RemoteRequestsPerMinute", 0, 0));
    llm_downloads_expanded = load_bool("LLMDownloadsExpanded", true);
    set_llm_storage_dir(config.getValue("Settings", "LlmStorageDir", ""));
    visual_model_id = normalize_visual_model_id(
        config.getValue("Settings", "VisualModelId", default_visual_model_descriptor().id));
    use_subcategories = load_bool("UseSubcategories", true);
    use_consistency_hints = load_bool("UseConsistencyHints", false);
    categorize_files = load_bool("CategorizeFiles", true);
    categorize_directories = load_bool("CategorizeDirectories", false);
    include_subdirectories = load_bool("IncludeSubdirectories", false);
    analyze_images_by_content = load_bool("AnalyzeImagesByContent", false);
    offer_rename_images = load_bool("OfferRenameImages", false);
    add_image_date_place_to_filename = load_bool("AddImageDatePlaceToFilename", false);
    add_audio_video_metadata_to_filename = load_bool("AddAudioVideoMetadataToFilename", true);
    add_image_date_to_category = load_bool("AddImageDateToCategory", false);
    rename_images_only = load_bool("RenameImagesOnly", false);
    process_images_only = load_bool("ProcessImagesOnly", false);
    analyze_documents_by_content = load_bool("AnalyzeDocumentsByContent", false);
    offer_rename_documents = load_bool("OfferRenameDocuments", false);
    rename_documents_only = load_bool("RenameDocumentsOnly", false);
    process_documents_only = load_bool("ProcessDocumentsOnly", false);
    add_document_date_to_category = load_bool("AddDocumentDateToCategory", false);
    const bool image_expand_default = process_images_only ||
                                      offer_rename_images ||
                                      rename_images_only ||
                                      add_image_date_place_to_filename ||
                                      add_image_date_to_category;
    if (config.hasValue("Settings", "ImageOptionsExpanded")) {
        image_options_expanded = load_bool("ImageOptionsExpanded", image_expand_default);
    } else {
        image_options_expanded = image_expand_default;
    }
    const bool document_expand_default = process_documents_only ||
                                         offer_rename_documents ||
                                         rename_documents_only ||
                                         add_document_date_to_category;
    if (config.hasValue("Settings", "DocumentOptionsExpanded")) {
        document_options_expanded = load_bool("DocumentOptionsExpanded", document_expand_default);
    } else {
        document_options_expanded = document_expand_default;
    }
    if (rename_images_only && !offer_rename_images) {
        offer_rename_images = true;
    }
    if (rename_documents_only && !offer_rename_documents) {
        offer_rename_documents = true;
    }
    if (include_subdirectories && categorize_directories) {
        categorize_directories = false;
    }
    sort_folder = config.getValue("Settings", "SortFolder", default_sort_folder.empty() ? std::string("/") : default_sort_folder);
    show_file_explorer = load_bool("ShowFileExplorer", true);
    recent_network_locations = parse_multiline_list(
        config.getValue("Settings", "RecentNetworkLocations", ""));
    suitability_benchmark_completed = load_bool("SuitabilityBenchmarkCompleted", false);
    suitability_benchmark_suppressed = load_bool("SuitabilityBenchmarkSuppressed", false);
    benchmark_last_report = decode_multiline(config.getValue("Settings", "BenchmarkLastReport", ""));
    benchmark_last_run = config.getValue("Settings", "BenchmarkLastRun", "");
    consistency_pass_enabled = load_bool("ConsistencyPass", false);
    development_prompt_logging = load_bool("DevelopmentPromptLogging", false);
    headless_review_before_apply = load_bool("HeadlessReviewBeforeApply", true);
    review_auto_approve_filename_changes =
        load_bool("ReviewAutoApproveFilenameChanges", false);
    review_auto_approve_categorization =
        load_bool("ReviewAutoApproveCategorization", false);
    skipped_version = config.getValue("Settings", "SkippedVersion", "0.0.0");
    whats_new_version_shown = config.getValue("Settings", "WhatsNewVersionShown", "");
    if (config.hasValue("Settings", "Language")) {
        language = languageFromString(QString::fromStdString(config.getValue("Settings", "Language", "English")));
    } else {
        language = system_default_language();
    }
    category_language = categoryLanguageFromString(QString::fromStdString(config.getValue("Settings", "CategoryLanguage", "English")));
    categorized_file_count = load_int("CategorizedFileCount", 0, 0);
    next_support_prompt_threshold = load_int("SupportPromptThreshold", 50, 50);
    windows_explorer_extension_prompt_dismissed =
        load_bool("WindowsExplorerExtensionPromptDismissed", false);
    windows_explorer_extension_prompt_last_shown_utc =
        config.getValue("Settings", "WindowsExplorerExtensionPromptLastShownUtc", "");
}

void Settings::load_whitelist_settings(const std::function<bool(const char*, bool)>& load_bool)
{
    allowed_categories = parse_list(config.getValue("Settings", "AllowedCategories", ""));
    allowed_subcategories = parse_list(config.getValue("Settings", "AllowedSubcategories", ""));
    allowed_subcategories_by_category =
        parse_subcategory_map(config.getValue("Settings", "AllowedSubcategoriesByCategory", ""));
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
        entry.mmproj_path = config.getValue(section, "MmprojPath", "");
        if (!entry.name.empty() && !entry.path.empty()) {
            custom_llms.push_back(entry);
        }
    }
}

void Settings::load_custom_api_settings()
{
    active_custom_api_id = config.getValue("CustomApis", "ActiveCustomApiId", "");

    custom_api_endpoints.clear();
    const auto api_ids = parse_list(config.getValue("CustomApis", "CustomApiIds", ""));
    for (const auto& id : api_ids) {
        const std::string section = "CustomApi_" + id;
        CustomApiEndpoint entry;
        entry.id = id;
        entry.name = config.getValue(section, "Name", "");
        entry.description = config.getValue(section, "Description", "");
        entry.base_url = config.getValue(section, "BaseUrl", "");
        entry.api_key = config.getValue(section, "ApiKey", "");
        entry.model = config.getValue(section, "Model", "");
        entry.name = trim_copy(entry.name);
        entry.description = trim_copy(entry.description);
        entry.base_url = trim_copy(entry.base_url);
        entry.api_key = trim_copy(entry.api_key);
        entry.model = trim_copy(entry.model);
        if (is_valid_custom_api_endpoint(entry)) {
            custom_api_endpoints.push_back(entry);
        }
    }
}

void Settings::log_loaded_settings() const
{
    if (auto logger = Logger::get_logger("core_logger")) {
        logger->info("Loaded settings from '{}' (allowed categories: {}, allowed subcategories: {}, use whitelist: {}, active whitelist: '{}', custom llms: {}, custom apis: {}, category language: {}, visual model: '{}')",
                     config_path,
                     allowed_categories.size(),
                     allowed_subcategories.size(),
                     use_whitelist,
                     active_whitelist,
                     custom_llms.size(),
                     custom_api_endpoints.size(),
                     categoryLanguageDisplay(category_language),
                     visual_model_id);
    }
}

void Settings::save_core_settings()
{
    static const std::string settings_section = "Settings";

    config.setValue(settings_section, "LLMChoice", llm_choice_to_string(llm_choice));
    config.setValue(settings_section, "RemoteApiKey", openai_api_key);
    config.setValue(settings_section, "RemoteModel", openai_model.empty() ? "gpt-4o-mini" : openai_model);
    config.setValue(settings_section, "GeminiApiKey", gemini_api_key);
    config.setValue(settings_section, "GeminiModel", gemini_model.empty() ? "gemini-2.5-flash-lite" : gemini_model);
    config.setValue(settings_section, "RemoteRequestsPerMinute", std::to_string(remote_requests_per_minute));
    set_bool_setting(config, settings_section, "LLMDownloadsExpanded", llm_downloads_expanded);
    config.setValue(settings_section, "LlmStorageDir", llm_storage_dir);
    config.setValue(settings_section, "VisualModelId", normalize_visual_model_id(visual_model_id));
    set_bool_setting(config, settings_section, "UseSubcategories", use_subcategories);
    set_bool_setting(config, settings_section, "UseConsistencyHints", use_consistency_hints);
    set_bool_setting(config, settings_section, "CategorizeFiles", categorize_files);
    set_bool_setting(config, settings_section, "CategorizeDirectories", categorize_directories);
    set_bool_setting(config, settings_section, "IncludeSubdirectories", include_subdirectories);
    if (rename_images_only) {
        offer_rename_images = true;
    }
    if (rename_documents_only) {
        offer_rename_documents = true;
    }
    set_bool_setting(config, settings_section, "AnalyzeImagesByContent", analyze_images_by_content);
    set_bool_setting(config, settings_section, "OfferRenameImages", offer_rename_images);
    set_bool_setting(config, settings_section, "AddImageDatePlaceToFilename", add_image_date_place_to_filename);
    set_bool_setting(config, settings_section, "AddAudioVideoMetadataToFilename", add_audio_video_metadata_to_filename);
    set_bool_setting(config, settings_section, "AddImageDateToCategory", add_image_date_to_category);
    set_bool_setting(config, settings_section, "ImageOptionsExpanded", image_options_expanded);
    set_bool_setting(config, settings_section, "RenameImagesOnly", rename_images_only);
    set_bool_setting(config, settings_section, "ProcessImagesOnly", process_images_only);
    set_bool_setting(config, settings_section, "AnalyzeDocumentsByContent", analyze_documents_by_content);
    set_bool_setting(config, settings_section, "OfferRenameDocuments", offer_rename_documents);
    set_bool_setting(config, settings_section, "DocumentOptionsExpanded", document_options_expanded);
    set_bool_setting(config, settings_section, "RenameDocumentsOnly", rename_documents_only);
    set_bool_setting(config, settings_section, "ProcessDocumentsOnly", process_documents_only);
    set_bool_setting(config, settings_section, "AddDocumentDateToCategory", add_document_date_to_category);
    config.setValue(settings_section, "SortFolder", this->sort_folder);

    set_optional_setting(config, settings_section, "SkippedVersion", skipped_version);
    set_optional_setting(config, settings_section, "WhatsNewVersionShown", whats_new_version_shown);

    set_bool_setting(config, settings_section, "ShowFileExplorer", show_file_explorer);
    set_optional_setting(config,
                         settings_section,
                         "RecentNetworkLocations",
                         join_multiline_list(recent_network_locations));
    set_bool_setting(config, settings_section, "SuitabilityBenchmarkCompleted", suitability_benchmark_completed);
    set_bool_setting(config, settings_section, "SuitabilityBenchmarkSuppressed", suitability_benchmark_suppressed);
    set_optional_setting(config, settings_section, "BenchmarkLastReport", encode_multiline(benchmark_last_report));
    set_optional_setting(config, settings_section, "BenchmarkLastRun", benchmark_last_run);
    set_bool_setting(config, settings_section, "ConsistencyPass", consistency_pass_enabled);
    set_bool_setting(config, settings_section, "DevelopmentPromptLogging", development_prompt_logging);
    set_bool_setting(config, settings_section, "HeadlessReviewBeforeApply", headless_review_before_apply);
    set_bool_setting(config,
                     settings_section,
                     "ReviewAutoApproveFilenameChanges",
                     review_auto_approve_filename_changes);
    set_bool_setting(config,
                     settings_section,
                     "ReviewAutoApproveCategorization",
                     review_auto_approve_categorization);
    config.setValue(settings_section, "Language", languageToString(language).toStdString());
    config.setValue(settings_section, "CategoryLanguage", categoryLanguageToString(category_language).toStdString());
    config.setValue(settings_section, "CategorizedFileCount", std::to_string(categorized_file_count));
    config.setValue(settings_section, "SupportPromptThreshold", std::to_string(next_support_prompt_threshold));
    set_bool_setting(config,
                     settings_section,
                     "WindowsExplorerExtensionPromptDismissed",
                     windows_explorer_extension_prompt_dismissed);
    set_optional_setting(config,
                         settings_section,
                         "WindowsExplorerExtensionPromptLastShownUtc",
                         windows_explorer_extension_prompt_last_shown_utc);
}

void Settings::save_whitelist_settings()
{
    static const std::string settings_section = "Settings";

    config.setValue(settings_section, "AllowedCategories", join_list(allowed_categories));
    config.setValue(settings_section, "AllowedSubcategories", join_list(allowed_subcategories));
    config.setValue(settings_section,
                    "AllowedSubcategoriesByCategory",
                    join_subcategory_map(allowed_subcategories_by_category));
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
        config.setValue(section, "MmprojPath", entry.mmproj_path);
    }
    config.setValue(llm_section, "CustomIds", join_list(ids));
}

void Settings::save_custom_api_endpoints()
{
    static const std::string api_section = "CustomApis";

    set_optional_setting(config, api_section, "ActiveCustomApiId", active_custom_api_id);

    std::vector<std::string> ids;
    ids.reserve(custom_api_endpoints.size());
    for (const auto& entry : custom_api_endpoints) {
        if (!is_valid_custom_api_endpoint(entry)) {
            continue;
        }
        ids.push_back(entry.id);
        const std::string section = "CustomApi_" + entry.id;
        config.setValue(section, "Name", entry.name);
        config.setValue(section, "Description", entry.description);
        config.setValue(section, "BaseUrl", entry.base_url);
        config.setValue(section, "ApiKey", entry.api_key);
        config.setValue(section, "Model", entry.model);
    }
    config.setValue(api_section, "CustomApiIds", join_list(ids));
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
        set_llm_storage_dir("");
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
    load_custom_api_settings();
    log_loaded_settings();

    return true;
}


bool Settings::save()
{
    save_core_settings();
    save_whitelist_settings();
    save_custom_llms();
    save_custom_api_endpoints();
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

std::string Settings::get_openai_api_key() const
{
    return openai_api_key;
}

void Settings::set_openai_api_key(const std::string& key)
{
    auto trimmed = key;
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), not_space));
    trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), not_space).base(), trimmed.end());
    openai_api_key = trimmed;
}

std::string Settings::get_openai_model() const
{
    return openai_model;
}

void Settings::set_openai_model(const std::string& model)
{
    auto trimmed = model;
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), not_space));
    trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), not_space).base(), trimmed.end());
    if (trimmed.empty()) {
        trimmed = "gpt-4o-mini";
    }
    openai_model = trimmed;
}

std::string Settings::get_gemini_api_key() const
{
    return gemini_api_key;
}

void Settings::set_gemini_api_key(const std::string& key)
{
    auto trimmed = key;
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), not_space));
    trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), not_space).base(), trimmed.end());
    gemini_api_key = trimmed;
}

std::string Settings::get_gemini_model() const
{
    return gemini_model;
}

void Settings::set_gemini_model(const std::string& model)
{
    auto trimmed = model;
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), not_space));
    trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), not_space).base(), trimmed.end());
    if (trimmed.empty()) {
        trimmed = "gemini-2.5-flash-lite";
    }
    gemini_model = trimmed;
}

int Settings::get_remote_requests_per_minute() const
{
    return remote_requests_per_minute;
}

void Settings::set_remote_requests_per_minute(int value)
{
    remote_requests_per_minute = std::max(0, value);
}

bool Settings::get_llm_downloads_expanded() const
{
    return llm_downloads_expanded;
}

void Settings::set_llm_downloads_expanded(bool value)
{
    llm_downloads_expanded = value;
}

std::string Settings::get_llm_storage_dir() const
{
    return llm_storage_dir;
}

void Settings::set_llm_storage_dir(const std::string& path)
{
    llm_storage_dir = trim_copy(path);
    Utils::set_llm_storage_directory_override(llm_storage_dir);
}

std::string Settings::get_visual_model_id() const
{
    return visual_model_id;
}

void Settings::set_visual_model_id(const std::string& id)
{
    visual_model_id = normalize_visual_model_id(id);
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
    copy.name = trim_copy(copy.name);
    copy.description = trim_copy(copy.description);
    copy.path = trim_copy(copy.path);
    copy.mmproj_path = trim_copy(copy.mmproj_path);
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

std::string Settings::get_active_custom_api_id() const
{
    return active_custom_api_id;
}

void Settings::set_active_custom_api_id(const std::string& id)
{
    active_custom_api_id = id;
}

const std::vector<CustomApiEndpoint>& Settings::get_custom_api_endpoints() const
{
    return custom_api_endpoints;
}

CustomApiEndpoint Settings::find_custom_api_endpoint(const std::string& id) const
{
    const auto it = std::find_if(custom_api_endpoints.begin(), custom_api_endpoints.end(),
                                 [&id](const CustomApiEndpoint& item) { return item.id == id; });
    if (it != custom_api_endpoints.end()) {
        return *it;
    }
    return {};
}

std::string Settings::upsert_custom_api_endpoint(const CustomApiEndpoint& endpoint)
{
    CustomApiEndpoint copy = endpoint;
    if (copy.id.empty()) {
        copy.id = generate_custom_api_id();
    }
    copy.name = trim_copy(copy.name);
    copy.description = trim_copy(copy.description);
    copy.base_url = trim_copy(copy.base_url);
    copy.api_key = trim_copy(copy.api_key);
    copy.model = trim_copy(copy.model);
    const auto it = std::find_if(custom_api_endpoints.begin(), custom_api_endpoints.end(),
                                 [&copy](const CustomApiEndpoint& item) { return item.id == copy.id; });
    if (it != custom_api_endpoints.end()) {
        *it = copy;
    } else {
        custom_api_endpoints.push_back(copy);
    }
    return copy.id;
}

void Settings::remove_custom_api_endpoint(const std::string& id)
{
    custom_api_endpoints.erase(std::remove_if(custom_api_endpoints.begin(),
                                             custom_api_endpoints.end(),
                                             [&id](const CustomApiEndpoint& item) { return item.id == id; }),
                               custom_api_endpoints.end());
    if (active_custom_api_id == id) {
        active_custom_api_id.clear();
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

bool Settings::get_include_subdirectories() const
{
    return include_subdirectories;
}

void Settings::set_include_subdirectories(bool value)
{
    include_subdirectories = value;
}

bool Settings::get_analyze_images_by_content() const
{
    return analyze_images_by_content;
}

void Settings::set_analyze_images_by_content(bool value)
{
    analyze_images_by_content = value;
}

bool Settings::get_offer_rename_images() const
{
    return offer_rename_images;
}

void Settings::set_offer_rename_images(bool value)
{
    offer_rename_images = value;
}

bool Settings::get_add_image_date_place_to_filename() const
{
    return add_image_date_place_to_filename;
}

void Settings::set_add_image_date_place_to_filename(bool value)
{
    add_image_date_place_to_filename = value;
}

bool Settings::get_add_audio_video_metadata_to_filename() const
{
    return add_audio_video_metadata_to_filename;
}

void Settings::set_add_audio_video_metadata_to_filename(bool value)
{
    add_audio_video_metadata_to_filename = value;
}

bool Settings::get_add_image_date_to_category() const
{
    return add_image_date_to_category;
}

void Settings::set_add_image_date_to_category(bool value)
{
    add_image_date_to_category = value;
}

bool Settings::get_image_options_expanded() const
{
    return image_options_expanded;
}

void Settings::set_image_options_expanded(bool value)
{
    image_options_expanded = value;
}

bool Settings::get_rename_images_only() const
{
    return rename_images_only;
}

void Settings::set_rename_images_only(bool value)
{
    rename_images_only = value;
}

bool Settings::get_process_images_only() const
{
    return process_images_only;
}

void Settings::set_process_images_only(bool value)
{
    process_images_only = value;
}

bool Settings::get_analyze_documents_by_content() const
{
    return analyze_documents_by_content;
}

void Settings::set_analyze_documents_by_content(bool value)
{
    analyze_documents_by_content = value;
}

bool Settings::get_offer_rename_documents() const
{
    return offer_rename_documents;
}

void Settings::set_offer_rename_documents(bool value)
{
    offer_rename_documents = value;
}

bool Settings::get_document_options_expanded() const
{
    return document_options_expanded;
}

void Settings::set_document_options_expanded(bool value)
{
    document_options_expanded = value;
}

bool Settings::get_rename_documents_only() const
{
    return rename_documents_only;
}

void Settings::set_rename_documents_only(bool value)
{
    rename_documents_only = value;
}

bool Settings::get_process_documents_only() const
{
    return process_documents_only;
}

void Settings::set_process_documents_only(bool value)
{
    process_documents_only = value;
}

bool Settings::get_add_document_date_to_category() const
{
    return add_document_date_to_category;
}

void Settings::set_add_document_date_to_category(bool value)
{
    add_document_date_to_category = value;
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

bool Settings::get_headless_review_before_apply() const
{
    return headless_review_before_apply;
}

void Settings::set_headless_review_before_apply(bool value)
{
    headless_review_before_apply = value;
}

bool Settings::get_review_auto_approve_filename_changes() const
{
    return review_auto_approve_filename_changes;
}

void Settings::set_review_auto_approve_filename_changes(bool value)
{
    review_auto_approve_filename_changes = value;
}

bool Settings::get_review_auto_approve_categorization() const
{
    return review_auto_approve_categorization;
}

void Settings::set_review_auto_approve_categorization(bool value)
{
    review_auto_approve_categorization = value;
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

void Settings::set_whats_new_version_shown(const std::string& version)
{
    whats_new_version_shown = trim_copy(version);
}

std::string Settings::get_whats_new_version_shown() const
{
    return whats_new_version_shown;
}


void Settings::set_show_file_explorer(bool value)
{
    show_file_explorer = value;
}


bool Settings::get_show_file_explorer() const
{
    return show_file_explorer;
}

void Settings::set_recent_network_locations(const std::vector<std::string>& locations)
{
    recent_network_locations.clear();
    recent_network_locations.reserve(std::min<std::size_t>(locations.size(), 10));
    for (const auto& location : locations) {
        const std::string trimmed = trim_copy(location);
        if (trimmed.empty()) {
            continue;
        }
        const auto duplicate = std::find(recent_network_locations.begin(),
                                         recent_network_locations.end(),
                                         trimmed);
        if (duplicate == recent_network_locations.end()) {
            recent_network_locations.push_back(trimmed);
        }
        if (recent_network_locations.size() >= 10) {
            break;
        }
    }
}

const std::vector<std::string>& Settings::get_recent_network_locations() const
{
    return recent_network_locations;
}

bool Settings::get_suitability_benchmark_completed() const
{
    return suitability_benchmark_completed;
}

bool Settings::get_suitability_benchmark_suppressed() const
{
    return suitability_benchmark_suppressed;
}

void Settings::set_suitability_benchmark_completed(bool value)
{
    suitability_benchmark_completed = value;
}

void Settings::set_suitability_benchmark_suppressed(bool value)
{
    suitability_benchmark_suppressed = value;
}

std::string Settings::get_benchmark_last_report() const
{
    return benchmark_last_report;
}

void Settings::set_benchmark_last_report(const std::string& value)
{
    benchmark_last_report = value;
}

std::string Settings::get_benchmark_last_run() const
{
    return benchmark_last_run;
}

void Settings::set_benchmark_last_run(const std::string& value)
{
    benchmark_last_run = value;
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
    if (threshold < 50) {
        threshold = 50;
    }
    next_support_prompt_threshold = threshold;
}

bool Settings::get_windows_explorer_extension_prompt_dismissed() const
{
    return windows_explorer_extension_prompt_dismissed;
}

void Settings::set_windows_explorer_extension_prompt_dismissed(bool value)
{
    windows_explorer_extension_prompt_dismissed = value;
}

std::string Settings::get_windows_explorer_extension_prompt_last_shown_utc() const
{
    return windows_explorer_extension_prompt_last_shown_utc;
}

void Settings::set_windows_explorer_extension_prompt_last_shown_utc(const std::string& value)
{
    windows_explorer_extension_prompt_last_shown_utc = value;
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

std::unordered_map<std::string, std::vector<std::string>>
Settings::get_allowed_subcategories_by_category() const
{
    return allowed_subcategories_by_category;
}

void Settings::set_allowed_subcategories_by_category(
    std::unordered_map<std::string, std::vector<std::string>> values)
{
    allowed_subcategories_by_category = std::move(values);
}
