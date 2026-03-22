#pragma once

#include <filesystem>
#include <string>

class UpdateArchiveExtractor
{
public:
    struct ExtractionResult
    {
        std::filesystem::path installer_path;
        std::string message;

        static ExtractionResult success(std::filesystem::path path)
        {
            return ExtractionResult{std::move(path), {}};
        }

        static ExtractionResult failure(std::string error)
        {
            return ExtractionResult{{}, std::move(error)};
        }

        bool ok() const
        {
            return !installer_path.empty();
        }
    };

    static bool supports_archive(const std::filesystem::path& package_path);
    static ExtractionResult extract_installer(const std::filesystem::path& archive_path,
                                              const std::filesystem::path& destination_root);
};
