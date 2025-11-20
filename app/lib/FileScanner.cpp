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

struct FileScanner::ScanContext {
    bool include_files{false};
    bool include_directories{false};
    bool include_hidden{false};
    std::shared_ptr<spdlog::logger> logger;
};

std::vector<FileEntry>
FileScanner::get_directory_entries(const std::string &directory_path,
                                   FileScanOptions options)
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
    context.logger = logger;

    try {
        const fs::path scan_path = Utils::utf8_to_path(directory_path);
        for (const auto &entry : fs::directory_iterator(scan_path)) {
            if (auto entry_info = build_entry(entry, context)) {
                file_paths_and_names.push_back(std::move(*entry_info));
            }
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


bool FileScanner::is_file_hidden(const fs::path &path) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesW(path.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES) &&
           (attrs & FILE_ATTRIBUTE_HIDDEN);
#else
    return path.filename().string().starts_with(".");
#endif
}


bool FileScanner::is_junk_file(const std::string& name) {
    static const std::unordered_set<std::string> junk = {
        ".DS_Store", "Thumbs.db", "desktop.ini"
    };
    return junk.contains(name);
}


bool FileScanner::is_file_bundle(const fs::path& path) {
    static const std::unordered_set<std::string> bundle_extensions = {
        ".app", ".utm", ".vmwarevm", ".pvm", ".vbox", ".pkg", ".mpkg",
        ".prefPane", ".plugin", ".framework", ".kext", ".qlgenerator",
        ".mdimporter", ".wdgt", ".scptd", ".nib", ".xib"
    };
    if (!fs::is_directory(path)) return false;

    std::string ext = Utils::path_to_utf8(path.extension());
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    return bundle_extensions.contains(ext);
}

std::optional<FileEntry> FileScanner::build_entry(const fs::directory_entry& entry,
                                                  const ScanContext& context)
{
    const fs::path& entry_path = entry.path();
    std::string full_path = Utils::path_to_utf8(entry_path);
    std::string file_name = Utils::path_to_utf8(entry_path.filename());

    if (is_junk_file(file_name)) {
        return std::nullopt;
    }

    const bool hidden = is_file_hidden(entry_path);
    if (hidden && !context.include_hidden) {
        if (context.logger) {
            context.logger->trace("Skipping hidden entry '{}'", full_path);
        }
        return std::nullopt;
    }

    const bool bundle = is_file_bundle(entry_path);
    const bool is_file = bundle || fs::is_regular_file(entry);
    const bool is_directory = !bundle && fs::is_directory(entry);

    if (context.include_files && is_file) {
        return FileEntry{std::move(full_path), std::move(file_name), FileType::File};
    }
    if (context.include_directories && is_directory) {
        return FileEntry{std::move(full_path), std::move(file_name), FileType::Directory};
    }

    return std::nullopt;
}
