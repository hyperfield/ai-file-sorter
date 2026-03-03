/**
 * @file MediaRenameMetadataService.hpp
 * @brief Builds audio/video filename suggestions from embedded media metadata.
 */
#pragma once

#include <filesystem>
#include <optional>
#include <string>

/**
 * @brief Suggests audio/video filenames using conventional metadata ordering.
 *
 * The composed format is `year_artist_album_title.ext` (missing fields are omitted,
 * while preserving order).
 */
class MediaRenameMetadataService {
public:
    /**
     * @brief Parsed metadata fields used for audio/video filename composition.
     */
    struct MetadataFields {
        std::optional<std::string> year;
        std::optional<std::string> artist;
        std::optional<std::string> album;
        std::optional<std::string> title;
    };

    /**
     * @brief Proposes a filename for a supported audio/video file.
     * @param media_path Full path to the media file.
     * @return Suggested filename (including extension), or `std::nullopt` when no metadata-based
     *         improvement is available.
     */
    std::optional<std::string> suggest_name(const std::filesystem::path& media_path) const;

    /**
     * @brief Returns true when the file extension is recognized as audio/video media.
     * @param path Candidate media path.
     * @return True for supported audio/video extensions.
     */
    static bool is_supported_media(const std::filesystem::path& path);

    /**
     * @brief Composes `year_artist_album_title.ext` using normalized metadata fragments.
     * @param original_path Original file path used for extension + fallback stem.
     * @param metadata Metadata fields used for composition.
     * @return Composed filename, or the original filename when composition data is unavailable.
     */
    static std::string compose_filename(const std::filesystem::path& original_path,
                                        const MetadataFields& metadata);

private:
    /**
     * @brief Extracts audio/video metadata fields from the given media file.
     * @param media_path Full path to the media file.
     * @return Metadata fields on success; `std::nullopt` when unavailable.
     */
    static std::optional<MetadataFields> extract_metadata(const std::filesystem::path& media_path);

    /**
     * @brief Normalizes a metadata token into lowercase underscore-separated text.
     * @param value Raw metadata text.
     * @return Normalized slug, or empty string when no valid characters remain.
     */
    static std::string slugify(const std::string& value);

    /**
     * @brief Extracts a year in `YYYY` form from an arbitrary date string.
     * @param value Raw metadata date value.
     * @return Four-digit year when available.
     */
    static std::optional<std::string> normalize_year(const std::string& value);
};
