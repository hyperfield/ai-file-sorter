#ifndef USER_PROFILE_MANAGER_HPP
#define USER_PROFILE_MANAGER_HPP

#include "Types.hpp"
#include "DatabaseManager.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace spdlog { class logger; }

class UserProfileManager {
public:
    explicit UserProfileManager(DatabaseManager& db_manager,
                               std::shared_ptr<spdlog::logger> logger);
    
    // Initialize or load existing user profile
    bool initialize_profile(const std::string& user_id = "default");
    
    // Get current user profile
    UserProfile get_profile() const;
    
    // Analyze a folder and update user profile based on findings
    void analyze_and_update_from_folder(const std::string& folder_path,
                                       const std::vector<CategorizedFile>& files);
    
    // Get folder-specific insights
    FolderInsight get_folder_insight(const std::string& folder_path) const;
    
    // Get all folder insights
    std::vector<FolderInsight> get_all_folder_insights() const;
    
    // Generate user context string for LLM prompts
    std::string generate_user_context_for_llm() const;
    
    // Re-scan and update profile based on new data
    void refresh_profile();
    
    // Get top characteristics by confidence
    std::vector<UserCharacteristic> get_top_characteristics(int limit = 10) const;
    
    // Organizational template management
    std::vector<OrganizationalTemplate> get_suggested_templates_for_folder(
        const std::string& folder_path) const;
    
    void learn_organizational_template(const std::string& folder_path,
                                      const std::vector<CategorizedFile>& files);
    
    std::vector<OrganizationalTemplate> get_all_templates() const;

private:
    // Analyze file patterns to infer user characteristics
    void infer_characteristics_from_files(const std::vector<CategorizedFile>& files,
                                         const std::string& folder_path);
    
    // Extract hobbies from file categories
    void extract_hobbies(const std::vector<CategorizedFile>& files);
    
    // Detect work-related patterns
    void detect_work_patterns(const std::vector<CategorizedFile>& files);
    
    // Analyze organizational preferences
    void analyze_organization_style(const std::vector<CategorizedFile>& files,
                                   const std::string& folder_path);
    
    // Update or add a characteristic
    void update_characteristic(const std::string& trait_name,
                             const std::string& value,
                             float confidence,
                             const std::string& evidence);
    
    // Generate folder description
    std::string generate_folder_description(const std::string& folder_path,
                                           const std::vector<CategorizedFile>& files);
    
    // Calculate category distribution
    std::unordered_map<std::string, int> calculate_category_distribution(
        const std::vector<CategorizedFile>& files);
    
    // Determine usage pattern for folder
    std::string determine_usage_pattern(const std::vector<CategorizedFile>& files);
    
    // Template learning helpers
    std::string extract_template_name_from_path(const std::string& folder_path) const;
    bool is_similar_template(const OrganizationalTemplate& t1,
                            const OrganizationalTemplate& t2) const;
    void merge_similar_templates();
    
    // Save profile to database
    void save_profile();
    
    // Load profile from database
    void load_profile();
    
    DatabaseManager& db_manager_;
    std::shared_ptr<spdlog::logger> logger_;
    UserProfile current_profile_;
    bool profile_loaded_{false};
};

#endif // USER_PROFILE_MANAGER_HPP
