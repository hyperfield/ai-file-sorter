#include "ResultsCoordinator.hpp"

#include <algorithm>
#include <filesystem>

ResultsCoordinator::ResultsCoordinator(FileScanner& scanner)
    : scanner(scanner)
{
}

std::vector<FileEntry> ResultsCoordinator::list_directory(const std::string& directory,
                                                          FileScanOptions options) const
{
    return scanner.get_directory_entries(directory, options);
}

std::vector<FileEntry> ResultsCoordinator::find_files_to_categorize(
    const std::string& directory_path,
    FileScanOptions options,
    const std::unordered_set<std::string>& cached_files,
    bool use_full_path_keys) const
{
    std::vector<FileEntry> actual_files = list_directory(directory_path, options);

    std::vector<FileEntry> found_files;
    found_files.reserve(actual_files.size());

    for (const auto& entry : actual_files) {
        const std::string key = use_full_path_keys ? entry.full_path : entry.file_name;
        if (!cached_files.contains(key)) {
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
            [&entry, use_full_path_keys](const CategorizedFile& categorized_file) {
                if (categorized_file.type != entry.type) {
                    return false;
                }
                if (use_full_path_keys) {
                    const auto full_path = std::filesystem::path(categorized_file.file_path) /
                                           std::filesystem::path(categorized_file.file_name);
                    return full_path == std::filesystem::path(entry.full_path);
                }
                return categorized_file.file_name == entry.file_name;
            });

        if (it != categorized_files.end()) {
            files_to_sort.push_back(*it);
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
            const auto full_path = std::filesystem::path(file.file_path) /
                                   std::filesystem::path(file.file_name);
            file_names.insert(full_path.string());
        } else {
            file_names.insert(file.file_name);
        }
    }
    return file_names;
}
