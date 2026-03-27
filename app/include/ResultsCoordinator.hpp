#ifndef RESULTS_COORDINATOR_HPP
#define RESULTS_COORDINATOR_HPP

#include "Types.hpp"
#include "StorageProvider.hpp"

#include <unordered_set>
#include <vector>

/**
 * @brief Coordinates scan results and determines which files should be categorized or sorted.
 *
 * The coordinator relies on a storage provider to list directory contents and then
 * filters/merges those results against cached or newly categorized entries.
 */
class ResultsCoordinator {
public:
    /**
     * @brief Constructs a coordinator that uses the provided storage provider.
     * @param storage_provider Provider used to enumerate directory entries.
     */
    explicit ResultsCoordinator(IStorageProvider& storage_provider);

    /**
     * @brief Rebinds the coordinator to a different active storage provider.
     * @param storage_provider Provider used to enumerate directory entries.
     */
    void set_storage_provider(IStorageProvider& storage_provider);

    /**
     * @brief Lists entries in a directory using the provided scan options.
     * @param directory Directory path to scan.
     * @param options File scan options (files, directories, hidden files).
     * @return Vector of FileEntry objects for items found in the directory.
     */
    std::vector<FileEntry> list_directory(const std::string& directory,
                                          FileScanOptions options) const;

    /**
     * @brief Returns directory entries that are not present in the cached set.
     * @param directory_path Directory path to scan.
     * @param options File scan options (files, directories, hidden files).
     * @param cached_files Set of cached file names to exclude.
     * @return Vector of FileEntry objects that are not in the cache.
     */
    std::vector<FileEntry> find_files_to_categorize(const std::string& directory_path,
                                                    FileScanOptions options,
                                                    const std::unordered_set<std::string>& cached_files,
                                                    bool use_full_path_keys) const;

    /**
     * @brief Filters categorized results to those still present on disk.
     * @param directory_path Base directory path (kept for symmetry with caller context).
     * @param options File scan options (files, directories, hidden files).
     * @param actual_files Current directory entries to validate against.
     * @param categorized_files Categorized entries from cache or analysis.
     * @return Vector of CategorizedFile entries that still exist in the directory.
     */
    std::vector<CategorizedFile> compute_files_to_sort(const std::string& directory_path,
                                                       FileScanOptions options,
                                                       const std::vector<FileEntry>& actual_files,
                                                       const std::vector<CategorizedFile>& categorized_files,
                                                       bool use_full_path_keys) const;

    /**
     * @brief Extracts file names from categorized entries into a set.
     * @param categorized_files Categorized entries to process.
     * @return Set of file names contained in the categorized list.
     */
    std::unordered_set<std::string> extract_file_names(const std::vector<CategorizedFile>& categorized_files,
                                                       bool use_full_path_keys) const;

private:
    /**
     * @brief Provider used to read directory entries.
     */
    IStorageProvider* storage_provider_;
};

#endif
