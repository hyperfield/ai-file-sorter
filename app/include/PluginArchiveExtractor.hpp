#pragma once

#include <filesystem>
#include <string>

/**
 * @brief Safely extracts AI File Sorter plugin archives.
 */
class PluginArchiveExtractor {
public:
    /**
     * @brief Result of extracting a plugin archive.
     */
    struct ExtractionResult {
        std::filesystem::path extracted_root;
        std::filesystem::path manifest_path;
        std::string message;

        /**
         * @brief Creates a successful extraction result.
         * @param root Directory containing extracted package files.
         * @param manifest Extracted manifest.json path.
         * @return Successful extraction result.
         */
        static ExtractionResult success(std::filesystem::path root,
                                        std::filesystem::path manifest);

        /**
         * @brief Creates a failed extraction result.
         * @param error User-facing error text.
         * @return Failed extraction result.
         */
        static ExtractionResult failure(std::string error);

        /**
         * @brief Reports whether extraction succeeded.
         * @return True when root and manifest paths are available.
         */
        bool ok() const;
    };

    /**
     * @brief Checks whether an archive extension is supported.
     * @param package_path Candidate archive path.
     * @return True for .aifsplugin and .zip files.
     */
    static bool supports_archive(const std::filesystem::path& package_path);

    /**
     * @brief Extracts an archive into a destination directory.
     * @param archive_path Source archive path.
     * @param destination_root Destination directory for extracted files.
     * @return Extraction result with manifest path or error.
     */
    static ExtractionResult extract_archive(const std::filesystem::path& archive_path,
                                            const std::filesystem::path& destination_root);
};
