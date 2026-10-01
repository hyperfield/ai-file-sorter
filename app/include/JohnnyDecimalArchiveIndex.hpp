#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace JohnnyDecimalArchiveIndex {

/**
 * @brief One folder below a Johnny.Decimal category.
 */
struct Item {
    /** @brief Parsed item/code prefix when one is present, for example 21.01. */
    std::string id;
    /** @brief Human-readable folder label without the parsed prefix when possible. */
    std::string label;
    /** @brief Relative folder path using forward slashes. */
    std::string relative_path;
    /** @brief Nesting depth below the archive root. */
    int depth{0};
    /** @brief Child folders nested below this item. */
    std::vector<Item> children;
};

/**
 * @brief One valid Johnny.Decimal category folder inside an area.
 */
struct Category {
    /** @brief Two-digit category id, for example 21. */
    std::string id;
    /** @brief Human-readable category label. */
    std::string label;
    /** @brief Relative folder path using forward slashes. */
    std::string relative_path;
    /** @brief Nesting depth below the archive root. */
    int depth{0};
    /** @brief Item folders nested below this category. */
    std::vector<Item> items;
};

/**
 * @brief One valid Johnny.Decimal top-level area range.
 */
struct Area {
    /** @brief Area range id, for example 20-29. */
    std::string id;
    /** @brief Human-readable area label. */
    std::string label;
    /** @brief Relative folder path using forward slashes. */
    std::string relative_path;
    /** @brief Nesting depth below the archive root. */
    int depth{0};
    /** @brief Valid direct category folders inside this area. */
    std::vector<Category> categories;
    /** @brief Direct area children that are not valid categories. */
    std::vector<std::string> other_direct_children;
};

/**
 * @brief Read-only Johnny.Decimal archive index.
 */
struct Index {
    /** @brief Root folder requested for indexing. */
    std::filesystem::path root;
    /** @brief True when the root was an existing directory and could be scanned. */
    bool scanned{false};
    /** @brief True when the scan reached the configured retained-entry limit. */
    bool scan_limit_reached{false};
    /** @brief Valid Johnny.Decimal area ranges found at the archive root. */
    std::vector<Area> areas;
    /** @brief Top-level folders outside the recognized area structure. */
    std::vector<std::string> other_top_level;
};

/**
 * @brief Build a read-only Johnny.Decimal area/category/item index.
 * @param root Archive root to scan.
 * @param max_depth Maximum folder depth to scan below the root.
 * @param max_entries Maximum number of folder entries retained from the scan.
 * @return Structured archive index.
 */
Index build_index(const std::filesystem::path& root, int max_depth = 8, std::size_t max_entries = 500);

/**
 * @brief Count valid categories in an archive index.
 * @param index Archive index to inspect.
 * @return Number of valid categories.
 */
std::size_t category_count(const Index& index);

/**
 * @brief Count item folders in an archive index.
 * @param index Archive index to inspect.
 * @return Number of item folders below valid categories.
 */
std::size_t item_count(const Index& index);

/**
 * @brief Format an archive index as a readable plain-text map.
 * @param index Archive index to format.
 * @return Plain-text map of areas, categories, and items.
 */
std::string format_index(const Index& index);

}  // namespace JohnnyDecimalArchiveIndex
