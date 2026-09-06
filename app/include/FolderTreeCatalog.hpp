#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace FolderTreeCatalog {

/**
 * @brief Marker injected into LLM prompt context when folder-tree routing is active.
 */
inline constexpr std::string_view kPromptMarker = "[[AI_FILE_SORTER_FOLDER_TREE_MODE]]";

/**
 * @brief One existing folder that may be selected as a sorting target.
 */
struct Entry {
    /** @brief Path relative to the sorting root, using forward slashes. */
    std::string relative_path;
    /** @brief Nesting depth below the sorting root. */
    int depth{0};
};

/**
 * @brief Result of validating a target folder path.
 */
struct ValidationResult {
    bool valid{false};
    std::string normalized_path;
    std::string error;
};

/**
 * @brief Parsed folder-tree target returned by the LLM.
 */
struct Selection {
    std::string relative_path;
    bool suggested_new{false};
    bool exists{false};
};

/**
 * @brief Catalog of existing destination folders under a sorting root.
 */
class Catalog {
public:
    /**
     * @brief Builds a catalog by recursively scanning existing folders below a root.
     * @param root Sorting root whose child folders are valid existing targets.
     * @param max_depth Maximum folder depth to include, relative to root.
     * @param max_entries Maximum number of folder entries to retain.
     * @return Catalog of relative folder paths.
     */
    static Catalog scan(const std::filesystem::path& root,
                        int max_depth = 8,
                        std::size_t max_entries = 500);

    /**
     * @brief Construct an empty catalog.
     */
    Catalog() = default;

    /**
     * @brief Construct a catalog from known entries.
     * @param entries Existing folder entries.
     */
    explicit Catalog(std::vector<Entry> entries);

    /**
     * @brief Return all catalog entries.
     * @return Existing folder entries.
     */
    const std::vector<Entry>& entries() const { return entries_; }

    /**
     * @brief Return whether the catalog has no existing folder targets.
     * @return True when no entries are available.
     */
    bool empty() const { return entries_.empty(); }

    /**
     * @brief Find an existing folder by relative path.
     * @param relative_path Candidate relative path.
     * @return Canonical catalog path when found.
     */
    std::optional<std::string> find_existing(std::string_view relative_path) const;

    /**
     * @brief Rank existing folders for a prompt using filename/path token overlap.
     * @param item_name File or folder name being routed.
     * @param item_path Path/context for the item.
     * @param max_candidates Maximum number of candidates to return.
     * @return Existing folder entries ordered by rough relevance.
     */
    std::vector<Entry> ranked_candidates(const std::string& item_name,
                                         const std::string& item_path,
                                         std::size_t max_candidates) const;

private:
    std::vector<Entry> entries_;
};

/**
 * @brief Validate and normalize a folder path relative to a sorting root.
 * @param relative_path Candidate relative folder path.
 * @return Validation result with a normalized forward-slash path on success.
 */
ValidationResult validate_relative_folder_path(std::string_view relative_path);

/**
 * @brief Parse a model response as a folder-tree selection.
 * @param response Raw LLM response.
 * @param catalog Existing destination folders.
 * @param allow_new_folders True to accept safe paths not present in the catalog.
 * @return Parsed selection when the response is usable.
 */
std::optional<Selection> parse_selection(const std::string& response,
                                         const Catalog& catalog,
                                         bool allow_new_folders);

/**
 * @brief Build the prompt context used to route files into an existing folder tree.
 * @param catalog Existing folder catalog.
 * @param item_name File or folder name being routed.
 * @param item_path Path/context for the item.
 * @param allow_new_folders True to allow new folder suggestions.
 * @return Prompt context block.
 */
std::string build_prompt_context(const Catalog& catalog,
                                 const std::string& item_name,
                                 const std::string& item_path,
                                 bool allow_new_folders);

/**
 * @brief Derive compatibility category labels from a relative target folder path.
 * @param relative_path Normalized relative target folder path.
 * @return Category/subcategory pair for cache/history compatibility.
 */
std::pair<std::string, std::string> derive_category_pair(std::string_view relative_path);

} // namespace FolderTreeCatalog
