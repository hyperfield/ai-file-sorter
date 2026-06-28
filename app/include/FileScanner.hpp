#ifndef FILE_SCANNER_HPP
#define FILE_SCANNER_HPP

#include <filesystem>
#include <string>
#include <vector>
#include <optional>
#include "Types.hpp"
#include "ProtectedProjectDetector.hpp"

namespace fs = std::filesystem;

struct FileScannerBehavior {
    bool skip_reparse_points{false};
    bool protect_project_directories{true};
    std::vector<std::string> additional_junk_names;
    std::vector<std::string> junk_name_prefixes;
};

class FileScanner {
public:
    FileScanner() = default;
    std::vector<FileEntry>
        get_directory_entries(const std::string &directory_path,
                              FileScanOptions options,
                              const FileScannerBehavior& behavior = {}) const;

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
    /**
     * @brief Logs a protected project that was skipped during traversal.
     * @param context Active scan context.
     * @param match Protected project match.
     */
    void log_protected_project_skip(const ScanContext& context,
                                    const ProtectedProjectMatch& match) const;
    std::optional<FileEntry> build_entry(const fs::directory_entry& entry,
                                         const ScanContext& context) const;
    bool should_skip_entry(const fs::directory_entry& entry,
                           const fs::path& entry_path,
                           const std::string& file_name,
                           const ScanContext& context,
                           const std::string& full_path) const;
    std::optional<FileType> classify_entry(const fs::directory_entry& entry,
                                           bool bundle,
                                           bool is_directory,
                                           const ScanContext& context) const;
    bool is_reparse_point_or_symlink(const fs::directory_entry& entry) const;
    bool is_file_hidden(const fs::path &path) const;
    bool is_junk_file(const std::string& name) const;
    bool is_additional_junk_file(const std::string& name, const ScanContext& context) const;
    bool is_file_bundle(const fs::path& path, bool is_directory) const;
    /**
     * @brief Returns a strong protected-project match for a directory.
     * @param path Directory path to evaluate.
     * @param context Active scan context.
     * @return Protected project match when traversal should skip it.
     */
    std::optional<ProtectedProjectMatch> strong_protected_project_match(
        const fs::path& path,
        const ScanContext& context) const;
};

#endif
