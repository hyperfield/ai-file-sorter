#include "StoragePluginArchiveExtractor.hpp"

bool StoragePluginArchiveExtractor::supports_archive(const std::filesystem::path& package_path)
{
    return PluginArchiveExtractor::supports_archive(package_path);
}

StoragePluginArchiveExtractor::ExtractionResult StoragePluginArchiveExtractor::extract_archive(
    const std::filesystem::path& archive_path,
    const std::filesystem::path& destination_root)
{
    return PluginArchiveExtractor::extract_archive(archive_path, destination_root);
}
