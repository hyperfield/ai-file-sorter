#ifndef FILE_SCANNER_HPP
#define FILE_SCANNER_HPP

#include <filesystem>
#include <string>
#include <vector>
#include <optional>
#include "Types.hpp"

namespace fs = std::filesystem;

class FileScanner {
public:
    FileScanner() = default;
    std::vector<FileEntry>
        get_directory_entries(const std::string &directory_path,
                              FileScanOptions options) const;

private:
    struct ScanContext;
    void scan_non_recursive(const fs::path& scan_path,
                            const ScanContext& context,
                            std::vector<FileEntry>& results) const;
    void scan_recursive(const fs::path& scan_path,
                        const ScanContext& context,
                        std::vector<FileEntry>& results) const;
    void log_scan_warning(const ScanContext& context,
                          const fs::path& path,
                          const std::error_code& error,
                          const char* action) const;
    std::optional<FileEntry> build_entry(const fs::directory_entry& entry,
                                         const ScanContext& context) const;
    bool should_skip_entry(const fs::path& entry_path,
                           const std::string& file_name,
                           const ScanContext& context,
                           const std::string& full_path) const;
    std::optional<FileType> classify_entry(const fs::directory_entry& entry,
                                           bool bundle,
                                           bool is_directory,
                                           const ScanContext& context) const;
    bool is_file_hidden(const fs::path &path) const;
    bool is_junk_file(const std::string& name) const;
    bool is_file_bundle(const fs::path& path, bool is_directory) const;
};

#endif
