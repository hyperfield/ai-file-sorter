#ifndef RESULTS_COORDINATOR_HPP
#define RESULTS_COORDINATOR_HPP

#include "Types.hpp"
#include "FileScanner.hpp"

#include <unordered_set>
#include <vector>

class ResultsCoordinator {
public:
    explicit ResultsCoordinator(FileScanner& scanner);

    std::vector<FileEntry> list_directory(const std::string& directory,
                                          FileScanOptions options) const;

    std::vector<FileEntry> find_files_to_categorize(const std::string& directory_path,
                                                    FileScanOptions options,
                                                    const std::unordered_set<std::string>& cached_files) const;

    std::vector<CategorizedFile> compute_files_to_sort(const std::string& directory_path,
                                                       FileScanOptions options,
                                                       const std::vector<FileEntry>& actual_files,
                                                       const std::vector<CategorizedFile>& categorized_files) const;

    std::unordered_set<std::string> extract_file_names(const std::vector<CategorizedFile>& categorized_files) const;

private:
    FileScanner& scanner;
};

#endif
