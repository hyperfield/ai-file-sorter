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
