#include "ResultsCoordinator.hpp"

#include <algorithm>

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
    const std::unordered_set<std::string>& cached_files) const
{
    std::vector<FileEntry> actual_files = list_directory(directory_path, options);

    std::vector<FileEntry> found_files;
    found_files.reserve(actual_files.size());

    for (const auto& entry : actual_files) {
        if (!cached_files.contains(entry.file_name)) {
            found_files.push_back(entry);
        }
    }

    return found_files;
}

std::vector<CategorizedFile> ResultsCoordinator::compute_files_to_sort(
    const std::string& directory_path,
    FileScanOptions options,
    const std::vector<FileEntry>& actual_files,
    const std::vector<CategorizedFile>& categorized_files) const
{
    std::vector<CategorizedFile> files_to_sort;
    files_to_sort.reserve(actual_files.size());

    for (const auto& entry : actual_files) {
        const auto it = std::find_if(
            categorized_files.begin(),
            categorized_files.end(),
            [&entry](const CategorizedFile& categorized_file) {
                return categorized_file.file_name == entry.file_name
                       && categorized_file.type == entry.type;
            });

        if (it != categorized_files.end()) {
            files_to_sort.push_back(*it);
        }
    }

    return files_to_sort;
}

std::unordered_set<std::string> ResultsCoordinator::extract_file_names(
    const std::vector<CategorizedFile>& categorized_files) const
{
    std::unordered_set<std::string> file_names;
    file_names.reserve(categorized_files.size());
    for (const auto& file : categorized_files) {
        file_names.insert(file.file_name);
    }
    return file_names;
}
