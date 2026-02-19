#ifndef IMAGE_RENAME_METADATA_SERVICE_HPP
#define IMAGE_RENAME_METADATA_SERVICE_HPP

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>

struct sqlite3;

/**
 * @brief Enriches image rename suggestions using EXIF date + reverse-geocoded place.
 *
 * Metadata sources:
 * - JPEG APP1 EXIF
 * - TIFF native EXIF
 * - PNG eXIf chunk
 * - HEIC/HEIF via exiftool fallback (when available in PATH)
 */
class ImageRenameMetadataService {
public:
    explicit ImageRenameMetadataService(std::string config_dir);
    ~ImageRenameMetadataService();

    ImageRenameMetadataService(const ImageRenameMetadataService&) = delete;
    ImageRenameMetadataService& operator=(const ImageRenameMetadataService&) = delete;

    /**
     * @brief Adds metadata prefixes to a suggested image filename when available.
     *
     * Prefix order is date first, then place, e.g. `2014-03-10_venice_black_ducks.jpg`.
     * If EXIF metadata is missing or place lookup cannot be done, the original
     * suggestion is returned unchanged.
     */
    std::string enrich_suggested_name(const std::filesystem::path& image_path,
                                      const std::string& suggested_name);

    /**
     * @brief Extracts the normalized capture date (`YYYY-MM-DD`) from image metadata.
     *
     * Returns `std::nullopt` when no supported metadata date is available.
     */
    std::optional<std::string> extract_capture_date(
        const std::filesystem::path& image_path) const;

    /**
     * @brief Utility used by tests and callers to compose prefixed filenames.
     */
    static std::string apply_prefix_to_filename(const std::string& suggested_name,
                                                const std::optional<std::string>& date_prefix,
                                                const std::optional<std::string>& place_prefix);

private:
    struct ExifMetadata {
        std::optional<std::string> capture_date;
        std::optional<double> latitude;
        std::optional<double> longitude;
    };

    struct CacheLookup {
        bool found{false};
        std::optional<std::string> place;
    };

    struct ReverseGeocodeResult {
        bool success{false};
        std::optional<std::string> place;
    };

    bool open_cache_db();
    ExifMetadata extract_exif_metadata(const std::filesystem::path& image_path) const;
    std::optional<std::string> resolve_place_prefix(double latitude, double longitude);
    CacheLookup lookup_cache(const std::string& lat_key, const std::string& lon_key) const;
    void upsert_cache(const std::string& lat_key,
                      const std::string& lon_key,
                      const std::optional<std::string>& place) const;
    ReverseGeocodeResult reverse_geocode(double latitude, double longitude);
    bool network_available();

    static std::string slugify(const std::string& value);
    static std::optional<std::string> normalize_exif_date(const std::string& value);
    static std::string format_coord_key(double value);

    std::string config_dir_;
    sqlite3* cache_db_{nullptr};
    std::chrono::steady_clock::time_point last_geocode_request_{};
    bool network_checked_{false};
    bool network_available_{false};
};

#endif // IMAGE_RENAME_METADATA_SERVICE_HPP
