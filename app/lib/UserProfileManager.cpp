#include "UserProfileManager.hpp"
#include "Logger.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <unordered_set>

namespace {
    // Confidence calculation constants
    constexpr float kMinHobbyConfidence = 0.3f;
    constexpr float kConfidenceIncrement = 0.05f;
    constexpr int kMinFilesForHobby = 3;
    
    constexpr float kMinWorkConfidence = 0.4f;
    constexpr float kWorkConfidenceIncrement = 0.03f;
    constexpr int kMinFilesForWork = 5;
    
    constexpr float kExistingCharacteristicWeight = 0.7f;
    constexpr float kNewCharacteristicWeight = 0.3f;
    
    // Template confidence constants
    constexpr float kTemplateBaseConfidence = 0.5f;  // Starting confidence for new template
    constexpr float kTemplateFileIncrement = 0.01f;  // Confidence boost per file in folder
    constexpr float kTemplateMaxConfidence = 0.95f;  // Cap to leave room for growth

    std::string get_current_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        // Use thread-safe localtime_r on Unix systems
        #ifdef _WIN32
        std::tm tm_buf;
        localtime_s(&tm_buf, &time_t_now);
        ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
        #else
        std::tm tm_buf;
        localtime_r(&time_t_now, &tm_buf);
        ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
        #endif
        return ss.str();
    }
}

UserProfileManager::UserProfileManager(DatabaseManager& db_manager,
                                      std::shared_ptr<spdlog::logger> logger)
    : db_manager_(db_manager)
    , logger_(logger)
{
}

bool UserProfileManager::initialize_profile(const std::string& user_id) {
    try {
        current_profile_.user_id = user_id;
        load_profile();
        
        if (current_profile_.created_at.empty()) {
            current_profile_.created_at = get_current_timestamp();
            current_profile_.last_updated = current_profile_.created_at;
            save_profile();
        }
        
        profile_loaded_ = true;
        logger_->info("User profile initialized for user: {}", user_id);
        return true;
    } catch (const std::exception& e) {
        logger_->error("Failed to initialize user profile: {}", e.what());
        return false;
    }
}

UserProfile UserProfileManager::get_profile() const {
    return current_profile_;
}

void UserProfileManager::analyze_and_update_from_folder(
    const std::string& folder_path,
    const std::vector<CategorizedFile>& files)
{
    if (files.empty()) {
        return;
    }
    
    // Check folder inclusion level
    std::string inclusion_level = db_manager_.get_folder_inclusion_level(folder_path);
    
    if (inclusion_level == "none") {
        logger_->info("Skipping profile storage for excluded folder: {}", folder_path);
        return;
    }
    
    logger_->info("Analyzing folder for user profile (level: {}): {}", inclusion_level, folder_path);
    
    // For "full" level: infer characteristics from the files
    // For "partial" level: skip characteristic inference but still store folder stats
    if (inclusion_level == "full") {
        infer_characteristics_from_files(files, folder_path);
    }
    
    // Always store folder insights for both "full" and "partial" levels
    FolderInsight insight;
    insight.folder_path = folder_path;
    insight.description = generate_folder_description(folder_path, files);
    insight.file_count = static_cast<int>(files.size());
    insight.last_analyzed = get_current_timestamp();
    insight.usage_pattern = determine_usage_pattern(files);
    
    // Calculate dominant categories
    auto cat_dist = calculate_category_distribution(files);
    std::vector<std::pair<std::string, int>> sorted_cats(cat_dist.begin(), cat_dist.end());
    std::sort(sorted_cats.begin(), sorted_cats.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    std::stringstream ss;
    int count = 0;
    for (const auto& [cat, freq] : sorted_cats) {
        if (count++ >= 3) break;
        if (!ss.str().empty()) ss << ", ";
        ss << cat << " (" << freq << ")";
    }
    insight.dominant_categories = ss.str();
    
    // Update or add folder insight
    auto it = std::find_if(current_profile_.folder_insights.begin(),
                          current_profile_.folder_insights.end(),
                          [&folder_path](const FolderInsight& fi) {
                              return fi.folder_path == folder_path;
                          });
    
    if (it != current_profile_.folder_insights.end()) {
        *it = insight;
    } else {
        current_profile_.folder_insights.push_back(insight);
    }
    
    current_profile_.last_updated = get_current_timestamp();
    
    // Learn organizational template from this folder
    learn_organizational_template(folder_path, files);
    
    save_profile();
    
    logger_->info("User profile updated with insights from: {}", folder_path);
}

void UserProfileManager::infer_characteristics_from_files(
    const std::vector<CategorizedFile>& files,
    const std::string& folder_path)
{
    extract_hobbies(files);
    detect_work_patterns(files);
    analyze_organization_style(files, folder_path);
}

void UserProfileManager::extract_hobbies(const std::vector<CategorizedFile>& files) {
    std::unordered_map<std::string, int> hobby_indicators;
    
    // Map categories to potential hobbies
    for (const auto& file : files) {
        std::string cat_lower = file.category;
        std::transform(cat_lower.begin(), cat_lower.end(), cat_lower.begin(), ::tolower);
        
        if (cat_lower.find("music") != std::string::npos ||
            cat_lower.find("audio") != std::string::npos) {
            hobby_indicators["Music"]++;
        }
        if (cat_lower.find("photo") != std::string::npos ||
            cat_lower.find("image") != std::string::npos) {
            hobby_indicators["Photography"]++;
        }
        if (cat_lower.find("video") != std::string::npos ||
            cat_lower.find("movie") != std::string::npos) {
            hobby_indicators["Video Production/Watching"]++;
        }
        if (cat_lower.find("game") != std::string::npos) {
            hobby_indicators["Gaming"]++;
        }
        if (cat_lower.find("code") != std::string::npos ||
            cat_lower.find("programming") != std::string::npos ||
            cat_lower.find("development") != std::string::npos) {
            hobby_indicators["Programming"]++;
        }
        if (cat_lower.find("art") != std::string::npos ||
            cat_lower.find("design") != std::string::npos) {
            hobby_indicators["Art & Design"]++;
        }
        if (cat_lower.find("read") != std::string::npos ||
            cat_lower.find("book") != std::string::npos ||
            cat_lower.find("ebook") != std::string::npos) {
            hobby_indicators["Reading"]++;
        }
    }
    
    // Update characteristics based on hobby indicators
    for (const auto& [hobby, count] : hobby_indicators) {
        if (count >= kMinFilesForHobby) {  // Threshold for confidence
            float confidence = std::min(kMinHobbyConfidence + (count * kConfidenceIncrement), 1.0f);
            std::string evidence = "Found " + std::to_string(count) + 
                                  " files related to " + hobby;
            update_characteristic("hobby", hobby, confidence, evidence);
        }
    }
}

void UserProfileManager::detect_work_patterns(const std::vector<CategorizedFile>& files) {
    std::unordered_set<std::string> work_categories = {
        "document", "documents", "spreadsheet", "presentation",
        "report", "business", "work", "project", "meeting"
    };
    
    int work_file_count = 0;
    std::unordered_map<std::string, int> work_types;
    
    for (const auto& file : files) {
        std::string cat_lower = file.category;
        std::transform(cat_lower.begin(), cat_lower.end(), cat_lower.begin(), ::tolower);
        
        for (const auto& work_cat : work_categories) {
            if (cat_lower.find(work_cat) != std::string::npos) {
                work_file_count++;
                work_types[work_cat]++;
                break;
            }
        }
    }
    
    if (work_file_count >= kMinFilesForWork) {
        float confidence = std::min(kMinWorkConfidence + (work_file_count * kWorkConfidenceIncrement), 1.0f);
        std::string evidence = "Found " + std::to_string(work_file_count) +
                              " work-related files";
        update_characteristic("work_pattern", "Professional/Office Work", confidence, evidence);
        
        // Identify specific work type
        std::string dominant_work;
        int max_count = 0;
        for (const auto& [type, count] : work_types) {
            if (count > max_count) {
                max_count = count;
                dominant_work = type;
            }
        }
        if (!dominant_work.empty()) {
            update_characteristic("primary_work_type", dominant_work,
                                confidence * 0.8f, evidence);
        }
    }
}

void UserProfileManager::analyze_organization_style(
    const std::vector<CategorizedFile>& files,
    const std::string& folder_path)
{
    // Analyze how organized the user is based on categorization patterns
    auto cat_dist = calculate_category_distribution(files);
    
    float organization_score = 0.0f;
    std::string style;
    
    if (cat_dist.size() <= 3) {
        organization_score = 0.9f;
        style = "Minimalist - prefers few, broad categories";
    } else if (cat_dist.size() <= 7) {
        organization_score = 0.8f;
        style = "Balanced - uses moderate categorization";
    } else if (cat_dist.size() <= 15) {
        organization_score = 0.7f;
        style = "Detailed - prefers granular organization";
    } else {
        organization_score = 0.6f;
        style = "Power User - highly detailed categorization system";
    }
    
    std::string evidence = "Uses " + std::to_string(cat_dist.size()) +
                          " distinct categories in " + folder_path;
    update_characteristic("organization_style", style, organization_score, evidence);
}

void UserProfileManager::update_characteristic(
    const std::string& trait_name,
    const std::string& value,
    float confidence,
    const std::string& evidence)
{
    // Find existing characteristic
    auto it = std::find_if(current_profile_.characteristics.begin(),
                          current_profile_.characteristics.end(),
                          [&trait_name, &value](const UserCharacteristic& uc) {
                              return uc.trait_name == trait_name && uc.value == value;
                          });
    
    if (it != current_profile_.characteristics.end()) {
        // Update existing - boost confidence slightly but cap at 1.0
        it->confidence = std::min(
            it->confidence * kExistingCharacteristicWeight + confidence * kNewCharacteristicWeight,
            1.0f
        );
        it->evidence = evidence;
        it->timestamp = get_current_timestamp();
    } else {
        // Add new characteristic
        UserCharacteristic characteristic;
        characteristic.trait_name = trait_name;
        characteristic.value = value;
        characteristic.confidence = confidence;
        characteristic.evidence = evidence;
        characteristic.timestamp = get_current_timestamp();
        current_profile_.characteristics.push_back(characteristic);
    }
}

std::string UserProfileManager::generate_folder_description(
    const std::string& folder_path,
    const std::vector<CategorizedFile>& files)
{
    auto cat_dist = calculate_category_distribution(files);
    
    std::stringstream desc;
    desc << "Folder contains " << files.size() << " items across "
         << cat_dist.size() << " categories. ";
    
    // Add dominant categories
    std::vector<std::pair<std::string, int>> sorted_cats(cat_dist.begin(), cat_dist.end());
    std::sort(sorted_cats.begin(), sorted_cats.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    if (!sorted_cats.empty()) {
        desc << "Primarily ";
        for (size_t i = 0; i < std::min(size_t(3), sorted_cats.size()); ++i) {
            if (i > 0) desc << ", ";
            desc << sorted_cats[i].first;
        }
        desc << " content.";
    }
    
    return desc.str();
}

std::unordered_map<std::string, int> UserProfileManager::calculate_category_distribution(
    const std::vector<CategorizedFile>& files)
{
    std::unordered_map<std::string, int> distribution;
    for (const auto& file : files) {
        if (!file.category.empty()) {
            distribution[file.category]++;
        }
    }
    return distribution;
}

std::string UserProfileManager::determine_usage_pattern(
    const std::vector<CategorizedFile>& files)
{
    std::unordered_map<std::string, int> pattern_indicators;
    
    for (const auto& file : files) {
        std::string cat_lower = file.category;
        std::transform(cat_lower.begin(), cat_lower.end(), cat_lower.begin(), ::tolower);
        
        if (cat_lower.find("work") != std::string::npos ||
            cat_lower.find("business") != std::string::npos ||
            cat_lower.find("document") != std::string::npos) {
            pattern_indicators["work"]++;
        }
        if (cat_lower.find("personal") != std::string::npos ||
            cat_lower.find("photo") != std::string::npos ||
            cat_lower.find("family") != std::string::npos) {
            pattern_indicators["personal"]++;
        }
        if (cat_lower.find("archive") != std::string::npos ||
            cat_lower.find("backup") != std::string::npos ||
            cat_lower.find("old") != std::string::npos) {
            pattern_indicators["archive"]++;
        }
    }
    
    std::string dominant = "general";
    int max_count = 0;
    for (const auto& [pattern, count] : pattern_indicators) {
        if (count > max_count) {
            max_count = count;
            dominant = pattern;
        }
    }
    
    return dominant;
}

FolderInsight UserProfileManager::get_folder_insight(const std::string& folder_path) const {
    auto it = std::find_if(current_profile_.folder_insights.begin(),
                          current_profile_.folder_insights.end(),
                          [&folder_path](const FolderInsight& fi) {
                              return fi.folder_path == folder_path;
                          });
    
    if (it != current_profile_.folder_insights.end()) {
        return *it;
    }
    
    return FolderInsight{};  // Return empty insight
}

std::vector<FolderInsight> UserProfileManager::get_all_folder_insights() const {
    return current_profile_.folder_insights;
}

std::string UserProfileManager::generate_user_context_for_llm() const {
    if (current_profile_.characteristics.empty()) {
        return "";
    }
    
    std::stringstream context;
    context << "\n\nUser Profile Context:\n";
    
    // Get top characteristics
    auto top_chars = get_top_characteristics(5);
    
    if (!top_chars.empty()) {
        context << "The user appears to be interested in: ";
        for (size_t i = 0; i < top_chars.size(); ++i) {
            if (i > 0) context << ", ";
            context << top_chars[i].value;
        }
        context << ".\n";
    }
    
    // Add organization style
    auto org_it = std::find_if(current_profile_.characteristics.begin(),
                               current_profile_.characteristics.end(),
                               [](const UserCharacteristic& uc) {
                                   return uc.trait_name == "organization_style";
                               });
    if (org_it != current_profile_.characteristics.end()) {
        context << "Organization preference: " << org_it->value << ".\n";
    }
    
    return context.str();
}

void UserProfileManager::refresh_profile() {
    load_profile();
    logger_->info("User profile refreshed");
}

std::vector<UserCharacteristic> UserProfileManager::get_top_characteristics(int limit) const {
    auto sorted_chars = current_profile_.characteristics;
    
    std::sort(sorted_chars.begin(), sorted_chars.end(),
              [](const UserCharacteristic& a, const UserCharacteristic& b) {
                  return a.confidence > b.confidence;
              });
    
    if (sorted_chars.size() > static_cast<size_t>(limit)) {
        sorted_chars.resize(limit);
    }
    
    return sorted_chars;
}

void UserProfileManager::save_profile() {
    db_manager_.save_user_profile(current_profile_);
    logger_->debug("User profile saved to database");
}

void UserProfileManager::load_profile() {
    current_profile_ = db_manager_.load_user_profile(current_profile_.user_id);
    logger_->debug("User profile loaded from database");
}

void UserProfileManager::learn_organizational_template(
    const std::string& folder_path,
    const std::vector<CategorizedFile>& files)
{
    if (files.size() < 3) {
        // Not enough data to learn a meaningful template
        return;
    }
    
    // Extract categories and subcategories
    std::unordered_set<std::string> categories;
    std::unordered_set<std::string> subcategories;
    
    for (const auto& file : files) {
        if (!file.category.empty()) {
            categories.insert(file.category);
        }
        if (!file.subcategory.empty()) {
            subcategories.insert(file.subcategory);
        }
    }
    
    // Determine template name based on folder usage pattern
    std::string usage_pattern = determine_usage_pattern(files);
    std::string folder_name = extract_template_name_from_path(folder_path);
    std::string template_name = usage_pattern + "_" + folder_name;
    
    // Check if we already have a similar template
    OrganizationalTemplate new_template;
    new_template.template_name = template_name;
    new_template.description = generate_folder_description(folder_path, files);
    new_template.suggested_categories = std::vector<std::string>(categories.begin(), categories.end());
    new_template.suggested_subcategories = std::vector<std::string>(subcategories.begin(), subcategories.end());
    new_template.based_on_folders = folder_path;
    new_template.usage_count = 1;
    new_template.confidence = std::min(
        kTemplateBaseConfidence + (files.size() * kTemplateFileIncrement),
        kTemplateMaxConfidence
    );
    
    // Find if similar template exists
    bool found_similar = false;
    for (auto& existing_template : current_profile_.learned_templates) {
        if (is_similar_template(existing_template, new_template)) {
            // Update existing template
            existing_template.usage_count++;
            existing_template.confidence = std::min(
                existing_template.confidence * 0.7f + new_template.confidence * 0.3f,
                1.0f
            );
            existing_template.based_on_folders += ", " + folder_path;
            
            // Merge categories
            for (const auto& cat : new_template.suggested_categories) {
                if (std::find(existing_template.suggested_categories.begin(),
                            existing_template.suggested_categories.end(),
                            cat) == existing_template.suggested_categories.end()) {
                    existing_template.suggested_categories.push_back(cat);
                }
            }
            
            found_similar = true;
            logger_->info("Updated existing template: {}", existing_template.template_name);
            break;
        }
    }
    
    if (!found_similar) {
        current_profile_.learned_templates.push_back(new_template);
        logger_->info("Learned new organizational template: {}", template_name);
    }
    
    // Merge similar templates periodically
    if (current_profile_.learned_templates.size() > 5) {
        merge_similar_templates();
    }
}

std::vector<OrganizationalTemplate> UserProfileManager::get_suggested_templates_for_folder(
    const std::string& folder_path) const
{
    std::vector<OrganizationalTemplate> suggestions;
    
    // Extract folder name to find relevant templates
    std::string folder_name = extract_template_name_from_path(folder_path);
    std::transform(folder_name.begin(), folder_name.end(), folder_name.begin(), ::tolower);
    
    for (const auto& templ : current_profile_.learned_templates) {
        // Extract just the meaningful part of template name (after usage pattern prefix)
        std::string templ_name = templ.template_name;
        size_t underscore_pos = templ_name.find('_');
        if (underscore_pos != std::string::npos && underscore_pos < templ_name.length() - 1) {
            templ_name = templ_name.substr(underscore_pos + 1);
        }
        
        std::transform(templ_name.begin(), templ_name.end(), templ_name.begin(), ::tolower);
        
        // Check if template name parts match folder name or if template has high confidence
        if (templ_name.find(folder_name) != std::string::npos ||
            folder_name.find(templ_name) != std::string::npos ||
            templ.confidence > 0.7f) {  // High confidence templates are always suggested
            suggestions.push_back(templ);
        }
    }
    
    // Sort by confidence
    std::sort(suggestions.begin(), suggestions.end(),
              [](const OrganizationalTemplate& a, const OrganizationalTemplate& b) {
                  return a.confidence > b.confidence;
              });
    
    // Return top 3 suggestions
    if (suggestions.size() > 3) {
        suggestions.resize(3);
    }
    
    return suggestions;
}

std::vector<OrganizationalTemplate> UserProfileManager::get_all_templates() const {
    return current_profile_.learned_templates;
}

std::string UserProfileManager::extract_template_name_from_path(
    const std::string& folder_path) const
{
    // Extract just the folder name from the full path
    size_t last_slash = folder_path.find_last_of("/\\");
    if (last_slash != std::string::npos && last_slash < folder_path.length() - 1) {
        return folder_path.substr(last_slash + 1);
    }
    return folder_path;
}

bool UserProfileManager::is_similar_template(
    const OrganizationalTemplate& t1,
    const OrganizationalTemplate& t2) const
{
    // Avoid division by zero - if both templates have no categories, they're not meaningfully similar
    if (t1.suggested_categories.empty() && t2.suggested_categories.empty()) {
        return false;
    }
    
    // Templates are similar if they share significant category overlap
    int common_categories = 0;
    for (const auto& cat1 : t1.suggested_categories) {
        for (const auto& cat2 : t2.suggested_categories) {
            if (cat1 == cat2) {
                common_categories++;
            }
        }
    }
    
    size_t max_size = std::max(t1.suggested_categories.size(), t2.suggested_categories.size());
    if (max_size == 0) {
        return false;  // Safety check
    }
    
    float similarity = static_cast<float>(common_categories) / max_size;
    
    return similarity > 0.6f;  // 60% category overlap
}

void UserProfileManager::merge_similar_templates() {
    // Find and merge templates with high similarity
    for (size_t i = 0; i < current_profile_.learned_templates.size(); ++i) {
        for (size_t j = i + 1; j < current_profile_.learned_templates.size(); ) {
            if (is_similar_template(current_profile_.learned_templates[i],
                                   current_profile_.learned_templates[j])) {
                // Merge j into i
                current_profile_.learned_templates[i].usage_count +=
                    current_profile_.learned_templates[j].usage_count;
                current_profile_.learned_templates[i].confidence = std::min(
                    (current_profile_.learned_templates[i].confidence +
                     current_profile_.learned_templates[j].confidence) / 2.0f,
                    1.0f
                );
                
                // Erase j
                current_profile_.learned_templates.erase(
                    current_profile_.learned_templates.begin() + j);
                logger_->debug("Merged similar templates");
            } else {
                ++j;
            }
        }
    }
}
