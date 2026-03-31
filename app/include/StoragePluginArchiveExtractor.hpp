#pragma once

#include <filesystem>
#include <string>

class StoragePluginArchiveExtractor
{
public:
    struct ExtractionResult
    {
        std::filesystem::path extracted_root;
        std::filesystem::path manifest_path;
        std::string message;

        static ExtractionResult success(std::filesystem::path root, std::filesystem::path manifest)
        {
            return ExtractionResult{std::move(root), std::move(manifest), {}};
        }

        static ExtractionResult failure(std::string error)
        {
            return ExtractionResult{{}, {}, std::move(error)};
        }

        bool ok() const
        {
            return !extracted_root.empty() && !manifest_path.empty();
        }
    };

    static bool supports_archive(const std::filesystem::path& package_path);
    static ExtractionResult extract_archive(const std::filesystem::path& archive_path,
                                            const std::filesystem::path& destination_root);
};
