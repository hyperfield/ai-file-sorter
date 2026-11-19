#pragma once

#include "PluginArchiveExtractor.hpp"

#include <filesystem>
#include <string>

class StoragePluginArchiveExtractor
{
public:
    using ExtractionResult = PluginArchiveExtractor::ExtractionResult;

    static bool supports_archive(const std::filesystem::path& package_path);
    static ExtractionResult extract_archive(const std::filesystem::path& archive_path,
                                            const std::filesystem::path& destination_root);
};
