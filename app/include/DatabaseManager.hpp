#ifndef DATABASEMANAGER_HPP
#define DATABASEMANAGER_HPP

#include "Types.hpp"
#include <string>
#include <map>
#include <vector>
#include <unordered_map>
#include <optional>
#include <sqlite3.h>

class DatabaseManager {
public:
    explicit DatabaseManager(std::string config_dir);
    ~DatabaseManager();

    bool is_file_already_categorized(const std::string &file_name);
    struct ResolvedCategory {
        int taxonomy_id;
        std::string category;
        std::string subcategory;
    };

    ResolvedCategory resolve_category(const std::string& category,
                                      const std::string& subcategory);

    bool insert_or_update_file_with_categorization(const std::string& file_name,
                                                   const std::string& file_type,
                                                   const std::string& dir_path,
                                                   const ResolvedCategory& resolved,
                                                   bool used_consistency_hints);
    std::vector<std::string> get_dir_contents_from_db(const std::string &dir_path);
    bool remove_file_categorization(const std::string& dir_path,
                                    const std::string& file_name,
                                    const FileType file_type);
    std::vector<CategorizedFile> remove_empty_categorizations(const std::string& dir_path);

    std::vector<CategorizedFile> get_categorized_files(const std::string &directory_path);

    std::vector<std::string>
        get_categorization_from_db(const std::string& file_name, const FileType file_type);
    void increment_taxonomy_frequency(int taxonomy_id);
    std::vector<std::pair<std::string, std::string>>
        get_taxonomy_snapshot(std::size_t max_entries) const;
    std::vector<std::pair<std::string, std::string>>
        get_recent_categories_for_extension(const std::string& extension,
                                            FileType file_type,
                                            std::size_t limit) const;
    bool clear_directory_categorizations(const std::string& dir_path);
    std::optional<bool> get_directory_categorization_style(const std::string& dir_path) const;

private:
    struct TaxonomyEntry {
        int id;
        std::string category;
        std::string subcategory;
        std::string normalized_category;
        std::string normalized_subcategory;
    };

    void initialize_schema();
    void initialize_taxonomy_schema();
    void load_taxonomy_cache();
    std::string normalize_label(const std::string& input) const;
    static double string_similarity(const std::string& a, const std::string& b);
    static std::string make_key(const std::string& norm_category,
                                const std::string& norm_subcategory);
    std::pair<int, double> find_fuzzy_match(const std::string& norm_category,
                                            const std::string& norm_subcategory) const;
    int resolve_existing_taxonomy(const std::string& key,
                                   const std::string& norm_category,
                                   const std::string& norm_subcategory) const;
    ResolvedCategory build_resolved_category(int taxonomy_id,
                                             const std::string& fallback_category,
                                             const std::string& fallback_subcategory,
                                             const std::string& norm_category,
                                             const std::string& norm_subcategory);
    int create_taxonomy_entry(const std::string& category,
                              const std::string& subcategory,
                              const std::string& norm_category,
                              const std::string& norm_subcategory);
    int find_existing_taxonomy_id(const std::string& norm_category,
                                  const std::string& norm_subcategory) const;
    void ensure_alias_mapping(int taxonomy_id,
                              const std::string& norm_category,
                              const std::string& norm_subcategory);
    const TaxonomyEntry* find_taxonomy_entry(int taxonomy_id) const;

    std::map<std::string, std::string> cached_results;
    std::string get_cached_category(const std::string &file_name);
    void load_cache();
    bool file_exists_in_db(const std::string &file_name, const std::string &file_path);

    sqlite3* db;
    const std::string config_dir;
    const std::string db_file;
    std::vector<TaxonomyEntry> taxonomy_entries;
    std::unordered_map<std::string, int> canonical_lookup;
    std::unordered_map<std::string, int> alias_lookup;
    std::unordered_map<int, size_t> taxonomy_index;

    static bool is_duplicate_category(
        const std::vector<std::pair<std::string, std::string>>& results,
        const std::pair<std::string, std::string>& candidate);
    std::optional<std::pair<std::string, std::string>> build_recent_category_candidate(
        const char* file_name_text,
        const char* category_text,
        const char* subcategory_text,
        const std::string& normalized_extension,
        bool has_extension) const;
};

#endif
