#include "FileScanner.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <unordered_set>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;


std::vector<FileEntry>
FileScanner::get_directory_entries(const std::string &directory_path,
                                   FileScanOptions options)
{
    std::vector<FileEntry> file_paths_and_names;
    auto logger = Logger::get_logger("core_logger");

    if (logger) {
        logger->debug("Scanning directory '{}' with options mask {}", directory_path, static_cast<int>(options));
    }

    const bool include_files = has_flag(options, FileScanOptions::Files);
    const bool include_directories = has_flag(options, FileScanOptions::Directories);
    const bool include_hidden = has_flag(options, FileScanOptions::HiddenFiles);

    const auto resolve_type = [&](const fs::directory_entry& entry,
                                  const fs::path& entry_path,
                                  bool is_bundle) -> std::optional<FileType> {
        const bool is_file = is_bundle || fs::is_regular_file(entry);
        const bool is_directory = !is_bundle && fs::is_directory(entry);
        if (include_files && is_file) {
            return FileType::File;
        }
        if (include_directories && is_directory) {
            return FileType::Directory;
        }
        return std::nullopt;
    };

    try {
        const fs::path scan_path = Utils::utf8_to_path(directory_path);
        for (const auto &entry : fs::directory_iterator(scan_path)) {
            const fs::path& entry_path = entry.path();
            std::string full_path = Utils::path_to_utf8(entry_path);
            std::string file_name = Utils::path_to_utf8(entry_path.filename());
            const bool is_hidden = is_file_hidden(entry_path);

            if (is_junk_file(file_name)) {
                continue;
            }
            if (is_hidden && !include_hidden) {
                if (logger) {
                    logger->trace("Skipping hidden entry '{}'", full_path);
                }
                continue;
            }

            const bool bundle = is_file_bundle(entry_path);
            if (auto type = resolve_type(entry, entry_path, bundle)) {
                file_paths_and_names.push_back({full_path, file_name, *type});
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
