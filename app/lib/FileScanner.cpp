#include "FileScanner.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <optional>
#include <unordered_set>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {

constexpr fs::directory_options kIteratorOptions =
    fs::directory_options::skip_permission_denied;

} // namespace

struct FileScanner::ScanContext {
    bool include_files{false};
    bool include_directories{false};
    bool include_hidden{false};
    FileScannerBehavior behavior;
    std::shared_ptr<spdlog::logger> logger;
};

std::vector<FileEntry>
FileScanner::get_directory_entries(const std::string &directory_path,
                                   FileScanOptions options,
                                   const FileScannerBehavior& behavior) const
{
    std::vector<FileEntry> file_paths_and_names;
    auto logger = Logger::get_logger("core_logger");

    if (logger) {
        logger->debug("Scanning directory '{}' with options mask {}", directory_path, static_cast<int>(options));
    }

    ScanContext context;
    context.include_files = has_flag(options, FileScanOptions::Files);
    context.include_directories = has_flag(options, FileScanOptions::Directories);
    context.include_hidden = has_flag(options, FileScanOptions::HiddenFiles);
    context.behavior = behavior;
    context.logger = logger;
    const bool recursive = has_flag(options, FileScanOptions::Recursive);

    try {
        const fs::path scan_path = Utils::utf8_to_path(directory_path);
        if (!recursive) {
            scan_non_recursive(scan_path, context, file_paths_and_names);
        } else {
            scan_recursive(scan_path, context, file_paths_and_names);
        }
    } catch (const fs::filesystem_error& ex) {
        if (logger) {
            logger->warn("Error while scanning '{}': {}", directory_path, ex.what());
        }
        throw;
    }

    if (logger) {
        logger->info("Directory scan complete for '{}': {} item(s) queued", directory_path,
                     file_paths_and_names.size());
    }

    return file_paths_and_names;
}

void FileScanner::scan_non_recursive(const fs::path& scan_path,
                                     const ScanContext& context,
                                     std::vector<FileEntry>& results) const
{
    std::error_code ec;
    fs::directory_iterator it(scan_path, kIteratorOptions, ec);
    if (ec) {
        throw fs::filesystem_error("directory_iterator", scan_path, ec);
    }

    const fs::directory_iterator end;
    while (it != end) {
        fs::directory_entry entry = *it;

        std::error_code increment_ec;
        it.increment(increment_ec);

        if (auto entry_info = build_entry(entry, context)) {
            results.push_back(std::move(*entry_info));
        }

        if (increment_ec) {
            log_scan_warning(context, scan_path, increment_ec,
                             "Stopping scan of directory after filesystem error");
            break;
        }
    }
}

void FileScanner::scan_recursive(const fs::path& scan_path,
                                 const ScanContext& context,
                                 std::vector<FileEntry>& results) const
{
    std::vector<fs::path> pending_dirs;
    pending_dirs.push_back(scan_path);

    while (!pending_dirs.empty()) {
        const fs::path current_dir = pending_dirs.back();
        pending_dirs.pop_back();

        std::error_code open_ec;
        fs::directory_iterator it(current_dir, kIteratorOptions, open_ec);
        if (open_ec) {
            if (current_dir == scan_path) {
                throw fs::filesystem_error("directory_iterator", current_dir, open_ec);
            }
            log_scan_warning(context, current_dir, open_ec,
                             "Skipping directory after filesystem error");
            continue;
        }

        const fs::directory_iterator end;
        while (it != end) {
            fs::directory_entry entry = *it;
            const fs::path entry_path = entry.path();
            const std::string full_path = Utils::path_to_utf8(entry_path);
            const std::string file_name = Utils::path_to_utf8(entry_path.filename());

            std::error_code dir_ec;
            const bool is_directory = entry.is_directory(dir_ec);
            if (dir_ec) {
                log_scan_warning(context, entry_path, dir_ec,
                                 "Skipping entry after filesystem error");
            } else {
                const bool bundle = is_file_bundle(entry_path, is_directory);
                const bool skip_entry = should_skip_entry(entry, entry_path, file_name, context, full_path);
                if (!skip_entry) {
                    if (auto type = classify_entry(entry, bundle, is_directory, context)) {
                        results.push_back(FileEntry{full_path, file_name, *type});
                    }
                }
                if (is_directory && !bundle && !skip_entry) {
                    pending_dirs.push_back(entry_path);
                }
            }

            std::error_code increment_ec;
            it.increment(increment_ec);
            if (increment_ec) {
                log_scan_warning(context, current_dir, increment_ec,
                                 "Stopping scan of directory after filesystem error");
                break;
            }
        }
    }
}

void FileScanner::log_scan_warning(const ScanContext& context,
                                   const fs::path& path,
                                   const std::error_code& error,
                                   const char* action) const
{
    if (!context.logger) {
        return;
    }
    context.logger->warn("{} '{}': {}", action, Utils::path_to_utf8(path), error.message());
}


bool FileScanner::is_file_hidden(const fs::path &path) const {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesW(path.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES) &&
           (attrs & FILE_ATTRIBUTE_HIDDEN);
#else
    return path.filename().string().starts_with(".");
#endif
}


bool FileScanner::is_junk_file(const std::string& name) const {
    static const std::unordered_set<std::string> junk = {
        ".DS_Store", "Thumbs.db", "desktop.ini"
    };
    return junk.contains(name);
}

bool FileScanner::is_additional_junk_file(const std::string& name,
                                          const ScanContext& context) const
{
    if (std::find(context.behavior.additional_junk_names.begin(),
                  context.behavior.additional_junk_names.end(),
                  name) != context.behavior.additional_junk_names.end()) {
        return true;
    }

    return std::any_of(context.behavior.junk_name_prefixes.begin(),
                       context.behavior.junk_name_prefixes.end(),
                       [&name](const std::string& prefix) {
                           return !prefix.empty() && name.starts_with(prefix);
                       });
}


bool FileScanner::is_file_bundle(const fs::path& path, bool is_directory) const {
    static const std::unordered_set<std::string> bundle_extensions = {
        ".app", ".utm", ".vmwarevm", ".pvm", ".vbox", ".pkg", ".mpkg",
        ".prefPane", ".plugin", ".framework", ".kext", ".qlgenerator",
        ".mdimporter", ".wdgt", ".scptd", ".nib", ".xib"
    };
    if (!is_directory) {
        return false;
    }

    std::string ext = Utils::path_to_utf8(path.extension());
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    return bundle_extensions.contains(ext);
}

std::optional<FileEntry> FileScanner::build_entry(const fs::directory_entry& entry,
                                                  const ScanContext& context) const
{
    const fs::path& entry_path = entry.path();
    std::string full_path = Utils::path_to_utf8(entry_path);
    std::string file_name = Utils::path_to_utf8(entry_path.filename());

    if (should_skip_entry(entry, entry_path, file_name, context, full_path)) {
        return std::nullopt;
    }

    std::error_code dir_ec;
    const bool is_directory = entry.is_directory(dir_ec);
    if (dir_ec) {
        log_scan_warning(context, entry_path, dir_ec,
                         "Skipping entry after filesystem error");
        return std::nullopt;
    }

    const bool bundle = is_file_bundle(entry_path, is_directory);
    if (auto type = classify_entry(entry, bundle, is_directory, context)) {
        return FileEntry{std::move(full_path), std::move(file_name), *type};
    }
    return std::nullopt;
}

bool FileScanner::is_reparse_point_or_symlink(const fs::directory_entry& entry) const
{
#ifdef _WIN32
    const DWORD attrs = GetFileAttributesW(entry.path().c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_REPARSE_POINT)) {
        return true;
    }
#endif

    std::error_code ec;
    return fs::is_symlink(entry.symlink_status(ec)) && !ec;
}

bool FileScanner::should_skip_entry(const fs::directory_entry& entry,
                                    const fs::path& entry_path,
                                    const std::string& file_name,
                                    const ScanContext& context,
                                    const std::string& full_path) const
{
    if (is_junk_file(file_name)) {
        return true;
    }

    if (is_additional_junk_file(file_name, context)) {
        if (context.logger) {
            context.logger->trace("Skipping provider-specific entry '{}'", full_path);
        }
        return true;
    }

    if (is_file_hidden(entry_path) && !context.include_hidden) {
        if (context.logger) {
            context.logger->trace("Skipping hidden entry '{}'", full_path);
        }
        return true;
    }

    if (context.behavior.skip_reparse_points && is_reparse_point_or_symlink(entry)) {
        if (context.logger) {
            context.logger->trace("Skipping reparse or symlink entry '{}'", full_path);
        }
        return true;
    }

    return false;
}

std::optional<FileType> FileScanner::classify_entry(const fs::directory_entry& entry,
                                                    bool bundle,
                                                    bool is_directory,
                                                    const ScanContext& context) const
{
    std::error_code regular_file_ec;
    const bool is_regular_file = entry.is_regular_file(regular_file_ec);
    if (regular_file_ec) {
        log_scan_warning(context, entry.path(), regular_file_ec,
                         "Skipping entry after filesystem error");
        return std::nullopt;
    }

    const bool is_file = bundle || is_regular_file;
    if (context.include_files && is_file) {
        return FileType::File;
    }

    if (context.include_directories && !bundle && is_directory) {
        return FileType::Directory;
    }

    return std::nullopt;
}
