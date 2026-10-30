#include "ResultsCoordinator.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>

ResultsCoordinator::ResultsCoordinator(IStorageProvider& storage_provider)
    : storage_provider_(&storage_provider)
{
}

void ResultsCoordinator::set_storage_provider(IStorageProvider& storage_provider)
{
    storage_provider_ = &storage_provider;
}

std::string ResultsCoordinator::cache_key(const std::string& path) const
{
    return storage_provider_->comparison_key(path);
}

std::vector<FileEntry> ResultsCoordinator::list_directory(const std::string& directory,
                                                          FileScanOptions options) const
{
    assert(storage_provider_ != nullptr);
    return storage_provider_->list_directory(directory, options);
}

std::vector<FileEntry> ResultsCoordinator::find_files_to_categorize(
    const std::string& directory_path,
    FileScanOptions options,
    const std::unordered_set<std::string>& cached_files,
    bool use_full_path_keys) const
{
    std::vector<FileEntry> actual_files = list_directory(directory_path, options);
    std::unordered_set<std::string> cached_keys;
    cached_keys.reserve(cached_files.size());
    for (const auto& path : cached_files) {
        cached_keys.insert(cache_key(path));
    }

    std::vector<FileEntry> found_files;
    found_files.reserve(actual_files.size());

    for (const auto& entry : actual_files) {
        const std::string key = cache_key(use_full_path_keys ? entry.full_path : entry.file_name);
        if (!cached_keys.contains(key)) {
            found_files.push_back(entry);
        }
    }

    return found_files;
}

std::vector<CategorizedFile> ResultsCoordinator::compute_files_to_sort(
    const std::string& directory_path,
    FileScanOptions options,
    const std::vector<FileEntry>& actual_files,
    const std::vector<CategorizedFile>& categorized_files,
    bool use_full_path_keys) const
{
    (void)directory_path;
    (void)options;
    std::vector<CategorizedFile> files_to_sort;
    files_to_sort.reserve(actual_files.size());

    for (const auto& entry : actual_files) {
        const auto it = std::find_if(
            categorized_files.begin(),
            categorized_files.end(),
            [this, &entry, use_full_path_keys](const CategorizedFile& categorized_file) {
                if (categorized_file.type != entry.type) {
                    return false;
                }
                if (use_full_path_keys) {
                    const auto full_path = Utils::utf8_to_path(categorized_file.file_path) /
                                           Utils::utf8_to_path(categorized_file.file_name);
                    return cache_key(Utils::path_to_utf8(full_path)) == cache_key(entry.full_path);
                }
                return cache_key(categorized_file.file_name) == cache_key(entry.file_name);
            });

        if (it != categorized_files.end()) {
            files_to_sort.push_back(*it);
            files_to_sort.back().file_name = entry.file_name;
        }
    }

    return files_to_sort;
}

std::unordered_set<std::string> ResultsCoordinator::extract_file_names(
    const std::vector<CategorizedFile>& categorized_files,
    bool use_full_path_keys) const
{
    std::unordered_set<std::string> file_names;
    file_names.reserve(categorized_files.size());
    for (const auto& file : categorized_files) {
        if (use_full_path_keys) {
            const auto full_path = Utils::utf8_to_path(file.file_path) /
                                   Utils::utf8_to_path(file.file_name);
            file_names.insert(cache_key(Utils::path_to_utf8(full_path)));
        } else {
            file_names.insert(cache_key(file.file_name));
        }
    }
    return file_names;
}
