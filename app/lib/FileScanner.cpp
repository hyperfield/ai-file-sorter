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

    try {
        const fs::path scan_path = Utils::utf8_to_path(directory_path);
        for (const auto &entry : fs::directory_iterator(scan_path)) {
            const fs::path& entry_path = entry.path();
            std::string full_path = Utils::path_to_utf8(entry_path);
            std::string file_name = Utils::path_to_utf8(entry_path.filename());
            bool is_hidden = is_file_hidden(entry_path);
            bool should_add = false;
            FileType file_type;

            if (is_junk_file(file_name)) continue;

            if (is_file_bundle(entry_path)) {
                if (has_flag(options, FileScanOptions::Files) &&
                    (has_flag(options, FileScanOptions::HiddenFiles) || !is_hidden)) {
                    file_type = FileType::File;
                    should_add = true;
                }
            }
            else if (fs::is_regular_file(entry)) {
                if (has_flag(options, FileScanOptions::Files) &&
                    (has_flag(options, FileScanOptions::HiddenFiles) || !is_hidden)) {
                    file_type = FileType::File;
                    should_add = true;
                }
            }
            else if (fs::is_directory(entry)) {
                if (has_flag(options, FileScanOptions::Directories) &&
                    (has_flag(options, FileScanOptions::HiddenFiles) || !is_hidden)) {
                    file_type = FileType::Directory;
                    should_add = true;
                }
            }

            if (should_add) {
                file_paths_and_names.push_back({full_path, file_name, file_type});
            } else if (logger && is_hidden && !has_flag(options, FileScanOptions::HiddenFiles)) {
                logger->trace("Skipping hidden entry '{}'", full_path);
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
