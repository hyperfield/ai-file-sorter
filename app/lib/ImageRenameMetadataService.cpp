#include "ImageRenameMetadataService.hpp"

#include "Utils.hpp"

#include <curl/curl.h>

#if __has_include(<jsoncpp/json/json.h>)
#include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
#include <json/json.h>
#else
#error "jsoncpp headers not found. Install jsoncpp development files."
#endif

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <initializer_list>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr char kExifPrefix[] = "Exif\0\0";
constexpr char kDefaultNominatimUrl[] = "https://nominatim.openstreetmap.org/reverse";
constexpr std::chrono::seconds kMinReverseInterval{1};
constexpr std::array<uint8_t, 8> kPngSignature = {
    0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A
};
constexpr size_t kHeifProbeOutputLimit = 256 * 1024;

constexpr uint16_t kTagDateTime = 0x0132;
constexpr uint16_t kTagExifIfd = 0x8769;
constexpr uint16_t kTagGpsIfd = 0x8825;
constexpr uint16_t kTagDateTimeOriginal = 0x9003;
constexpr uint16_t kTagCreateDate = 0x9004;
constexpr uint16_t kTagGpsLatitudeRef = 0x0001;
constexpr uint16_t kTagGpsLatitude = 0x0002;
constexpr uint16_t kTagGpsLongitudeRef = 0x0003;
constexpr uint16_t kTagGpsLongitude = 0x0004;

struct TiffEntry {
    uint16_t tag{0};
    uint16_t type{0};
    uint32_t count{0};
    uint32_t value_or_offset{0};
    size_t raw_offset{0};
};

struct ParsedExifMetadata {
    std::optional<std::string> capture_date;
    std::optional<double> latitude;
    std::optional<double> longitude;
};

size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp)
{
    const size_t total = size * nmemb;
    auto* output = static_cast<std::string*>(userp);
    output->append(static_cast<const char*>(contents), total);
    return total;
}

void configure_tls(CURL* curl)
{
#if defined(_WIN32)
    const auto cert_path = Utils::ensure_ca_bundle();
    curl_easy_setopt(curl, CURLOPT_CAINFO, cert_path.string().c_str());
#else
    (void)curl;
#endif
}

bool read_byte(std::ifstream& in, uint8_t& value)
{
    char ch = 0;
    if (!in.get(ch)) {
        return false;
    }
    value = static_cast<uint8_t>(static_cast<unsigned char>(ch));
    return true;
}

bool read_exact(std::ifstream& in, uint8_t* data, size_t size)
{
    if (size == 0) {
        return true;
    }
    in.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(size));
    return in.good();
}

std::optional<std::vector<uint8_t>> read_jpeg_exif_payload(const std::filesystem::path& image_path)
{
    std::ifstream in(image_path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }

    uint8_t soi[2] = {0, 0};
    if (!read_exact(in, soi, sizeof(soi)) || soi[0] != 0xFF || soi[1] != 0xD8) {
        return std::nullopt;
    }

    while (true) {
        uint8_t prefix = 0;
        if (!read_byte(in, prefix)) {
            break;
        }
        if (prefix != 0xFF) {
            continue;
        }

        uint8_t marker = 0;
        do {
            if (!read_byte(in, marker)) {
                return std::nullopt;
            }
        } while (marker == 0xFF);

        if (marker == 0x00) {
            continue;
        }

        if (marker == 0xD9 || marker == 0xDA) {
            break;
        }

        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {
            continue;
        }

        uint8_t length_bytes[2] = {0, 0};
        if (!read_exact(in, length_bytes, sizeof(length_bytes))) {
            break;
        }

        const uint16_t segment_length =
            static_cast<uint16_t>((static_cast<uint16_t>(length_bytes[0]) << 8) | length_bytes[1]);
        if (segment_length < 2) {
            break;
        }

        const size_t payload_size = static_cast<size_t>(segment_length - 2);
        if (marker == 0xE1) {
            std::vector<uint8_t> payload(payload_size);
            if (!read_exact(in, payload.data(), payload.size())) {
                break;
            }

            if (payload.size() >= sizeof(kExifPrefix) - 1 &&
                std::memcmp(payload.data(), kExifPrefix, sizeof(kExifPrefix) - 1) == 0) {
                return std::vector<uint8_t>(payload.begin() + static_cast<std::ptrdiff_t>(sizeof(kExifPrefix) - 1),
                                            payload.end());
            }
            continue;
        }

        in.seekg(static_cast<std::streamoff>(payload_size), std::ios::cur);
        if (!in) {
            break;
        }
    }

    return std::nullopt;
}

std::optional<std::vector<uint8_t>> read_file_bytes(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
        return std::nullopt;
    }

    const std::streamoff size = in.tellg();
    if (size < 0) {
        return std::nullopt;
    }

    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    in.seekg(0, std::ios::beg);
    if (!read_exact(in, bytes.data(), bytes.size())) {
        return std::nullopt;
    }

    return bytes;
}

bool starts_with_bytes(const std::vector<uint8_t>& data, const char* prefix, size_t prefix_size)
{
    if (data.size() < prefix_size) {
        return false;
    }
    return std::memcmp(data.data(), prefix, prefix_size) == 0;
}

bool looks_like_tiff_payload(const std::vector<uint8_t>& data)
{
    if (data.size() < 4) {
        return false;
    }

    const bool little_endian = (data[0] == 'I' && data[1] == 'I');
    const bool big_endian = (data[0] == 'M' && data[1] == 'M');
    if (!little_endian && !big_endian) {
        return false;
    }

    if (little_endian) {
        return data[2] == 42 && data[3] == 0;
    }
    return data[2] == 0 && data[3] == 42;
}

std::optional<std::vector<uint8_t>> read_tiff_exif_payload(const std::filesystem::path& image_path)
{
    const auto bytes = read_file_bytes(image_path);
    if (!bytes || bytes->size() < 8) {
        return std::nullopt;
    }

    if (looks_like_tiff_payload(*bytes)) {
        return bytes;
    }

    if (starts_with_bytes(*bytes, kExifPrefix, sizeof(kExifPrefix) - 1)) {
        std::vector<uint8_t> payload(bytes->begin() + static_cast<std::ptrdiff_t>(sizeof(kExifPrefix) - 1),
                                     bytes->end());
        if (looks_like_tiff_payload(payload)) {
            return payload;
        }
    }

    return std::nullopt;
}

std::optional<std::vector<uint8_t>> read_png_exif_payload(const std::filesystem::path& image_path)
{
    std::ifstream in(image_path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }

    std::array<uint8_t, 8> signature{};
    if (!read_exact(in, signature.data(), signature.size()) || signature != kPngSignature) {
        return std::nullopt;
    }

    while (true) {
        uint8_t chunk_header[8] = {0};
        if (!read_exact(in, chunk_header, sizeof(chunk_header))) {
            return std::nullopt;
        }

        const uint32_t chunk_length =
            (static_cast<uint32_t>(chunk_header[0]) << 24) |
            (static_cast<uint32_t>(chunk_header[1]) << 16) |
            (static_cast<uint32_t>(chunk_header[2]) << 8) |
            static_cast<uint32_t>(chunk_header[3]);

        const std::array<char, 4> chunk_type = {
            static_cast<char>(chunk_header[4]),
            static_cast<char>(chunk_header[5]),
            static_cast<char>(chunk_header[6]),
            static_cast<char>(chunk_header[7])
        };

        std::vector<uint8_t> chunk_data(chunk_length);
        if (!read_exact(in, chunk_data.data(), chunk_data.size())) {
            return std::nullopt;
        }

        uint8_t crc[4] = {0};
        if (!read_exact(in, crc, sizeof(crc))) {
            return std::nullopt;
        }

        if (chunk_type == std::array<char, 4>{'e', 'X', 'I', 'f'}) {
            if (starts_with_bytes(chunk_data, kExifPrefix, sizeof(kExifPrefix) - 1)) {
                return std::vector<uint8_t>(chunk_data.begin() +
                                                static_cast<std::ptrdiff_t>(sizeof(kExifPrefix) - 1),
                                            chunk_data.end());
            }
            return chunk_data;
        }

        if (chunk_type == std::array<char, 4>{'I', 'E', 'N', 'D'}) {
            break;
        }
    }

    return std::nullopt;
}

std::string to_lower_ascii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string file_extension_lower(const std::filesystem::path& path)
{
    return to_lower_ascii(Utils::path_to_utf8(path.extension()));
}

bool has_extension(const std::string& extension, std::initializer_list<const char*> choices)
{
    for (const char* choice : choices) {
        if (extension == choice) {
            return true;
        }
    }
    return false;
}

size_t tiff_type_size(uint16_t type)
{
    switch (type) {
        case 1:  return 1; // BYTE
        case 2:  return 1; // ASCII
        case 3:  return 2; // SHORT
        case 4:  return 4; // LONG
        case 5:  return 8; // RATIONAL
        default: return 0;
    }
}

std::optional<uint16_t> read_u16(const std::vector<uint8_t>& data,
                                 size_t offset,
                                 bool little_endian)
{
    if (offset + 2 > data.size()) {
        return std::nullopt;
    }
    const uint16_t b0 = data[offset];
    const uint16_t b1 = data[offset + 1];
    if (little_endian) {
        return static_cast<uint16_t>((b1 << 8) | b0);
    }
    return static_cast<uint16_t>((b0 << 8) | b1);
}

std::optional<uint32_t> read_u32(const std::vector<uint8_t>& data,
                                 size_t offset,
                                 bool little_endian)
{
    if (offset + 4 > data.size()) {
        return std::nullopt;
    }

    const uint32_t b0 = data[offset];
    const uint32_t b1 = data[offset + 1];
    const uint32_t b2 = data[offset + 2];
    const uint32_t b3 = data[offset + 3];

    if (little_endian) {
        return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
    }
    return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
}

bool parse_ifd_entries(const std::vector<uint8_t>& tiff,
                       bool little_endian,
                       uint32_t ifd_offset,
                       std::vector<TiffEntry>& entries)
{
    entries.clear();
    if (ifd_offset + 2 > tiff.size()) {
        return false;
    }

    const auto count_opt = read_u16(tiff, ifd_offset, little_endian);
    if (!count_opt.has_value()) {
        return false;
    }

    const size_t entry_count = *count_opt;
    size_t cursor = static_cast<size_t>(ifd_offset + 2);
    if (cursor + entry_count * 12 > tiff.size()) {
        return false;
    }

    entries.reserve(entry_count);
    for (size_t i = 0; i < entry_count; ++i) {
        const size_t entry_offset = cursor + i * 12;

        const auto tag_opt = read_u16(tiff, entry_offset, little_endian);
        const auto type_opt = read_u16(tiff, entry_offset + 2, little_endian);
        const auto count_value_opt = read_u32(tiff, entry_offset + 4, little_endian);
        const auto value_opt = read_u32(tiff, entry_offset + 8, little_endian);

        if (!tag_opt || !type_opt || !count_value_opt || !value_opt) {
            return false;
        }

        entries.push_back(TiffEntry{*tag_opt, *type_opt, *count_value_opt, *value_opt, entry_offset});
    }

    return true;
}

const TiffEntry* find_entry(const std::vector<TiffEntry>& entries, uint16_t tag)
{
    for (const auto& entry : entries) {
        if (entry.tag == tag) {
            return &entry;
        }
    }
    return nullptr;
}

std::optional<size_t> value_data_offset(const std::vector<uint8_t>& tiff,
                                        const TiffEntry& entry)
{
    const size_t unit = tiff_type_size(entry.type);
    if (unit == 0) {
        return std::nullopt;
    }

    const size_t value_size = static_cast<size_t>(entry.count) * unit;
    if (value_size == 0) {
        return std::nullopt;
    }

    if (value_size <= 4) {
        const size_t inline_offset = entry.raw_offset + 8;
        if (inline_offset + value_size > tiff.size()) {
            return std::nullopt;
        }
        return inline_offset;
    }

    const size_t data_offset = static_cast<size_t>(entry.value_or_offset);
    if (data_offset + value_size > tiff.size()) {
        return std::nullopt;
    }
    return data_offset;
}

std::string trim_copy(std::string value)
{
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::optional<std::string> read_ascii_value(const std::vector<uint8_t>& tiff,
                                            const TiffEntry& entry)
{
    if (entry.type != 2 || entry.count == 0) {
        return std::nullopt;
    }

    const auto data_offset = value_data_offset(tiff, entry);
    if (!data_offset) {
        return std::nullopt;
    }

    size_t length = static_cast<size_t>(entry.count);
    if (length == 0) {
        return std::nullopt;
    }

    const char* begin = reinterpret_cast<const char*>(tiff.data() + *data_offset);
    const char* end = begin + length;

    const char* first_null = std::find(begin, end, '\0');
    std::string value(begin, first_null);
    value = trim_copy(value);
    if (value.empty()) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::array<double, 3>> read_rational_triplet(const std::vector<uint8_t>& tiff,
                                                            const TiffEntry& entry,
                                                            bool little_endian)
{
    if (entry.type != 5 || entry.count < 3) {
        return std::nullopt;
    }

    const auto data_offset = value_data_offset(tiff, entry);
    if (!data_offset) {
        return std::nullopt;
    }

    std::array<double, 3> values{};
    for (size_t i = 0; i < 3; ++i) {
        const size_t pair_offset = *data_offset + i * 8;
        const auto numerator = read_u32(tiff, pair_offset, little_endian);
        const auto denominator = read_u32(tiff, pair_offset + 4, little_endian);
        if (!numerator || !denominator || *denominator == 0) {
            return std::nullopt;
        }
        values[i] = static_cast<double>(*numerator) / static_cast<double>(*denominator);
    }

    return values;
}

std::optional<double> decode_gps_coordinate(const std::array<double, 3>& dms,
                                            char hemisphere)
{
    const double degrees = dms[0];
    const double minutes = dms[1];
    const double seconds = dms[2];

    if (!std::isfinite(degrees) || !std::isfinite(minutes) || !std::isfinite(seconds)) {
        return std::nullopt;
    }

    double value = degrees + (minutes / 60.0) + (seconds / 3600.0);
    if (hemisphere == 'S' || hemisphere == 'W') {
        value = -value;
    }

    if (!std::isfinite(value)) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::string> normalize_exif_date_value(const std::string& value)
{
    static const std::regex kDatePattern(R"((\d{4})[:\-](\d{2})[:\-](\d{2}))");
    std::smatch match;
    if (!std::regex_search(value, match, kDatePattern)) {
        return std::nullopt;
    }

    const std::string year = match.str(1);
    const std::string month = match.str(2);
    const std::string day = match.str(3);

    if (year.size() != 4 || month.size() != 2 || day.size() != 2) {
        return std::nullopt;
    }

    return year + "-" + month + "-" + day;
}

ParsedExifMetadata parse_tiff_metadata(const std::vector<uint8_t>& tiff)
{
    ParsedExifMetadata metadata;
    if (tiff.size() < 8) {
        return metadata;
    }

    bool little_endian = false;
    if (tiff[0] == 'I' && tiff[1] == 'I') {
        little_endian = true;
    } else if (tiff[0] == 'M' && tiff[1] == 'M') {
        little_endian = false;
    } else {
        return metadata;
    }

    const auto magic = read_u16(tiff, 2, little_endian);
    if (!magic || *magic != 42) {
        return metadata;
    }

    const auto ifd0_offset = read_u32(tiff, 4, little_endian);
    if (!ifd0_offset) {
        return metadata;
    }

    std::vector<TiffEntry> ifd0_entries;
    if (!parse_ifd_entries(tiff, little_endian, *ifd0_offset, ifd0_entries)) {
        return metadata;
    }

    if (const TiffEntry* dt = find_entry(ifd0_entries, kTagDateTime)) {
        if (const auto parsed = read_ascii_value(tiff, *dt)) {
            metadata.capture_date = normalize_exif_date_value(*parsed);
        }
    }

    uint32_t exif_ifd_offset = 0;
    if (const TiffEntry* exif_ptr = find_entry(ifd0_entries, kTagExifIfd)) {
        if (exif_ptr->count == 1 && exif_ptr->type == 4) {
            exif_ifd_offset = exif_ptr->value_or_offset;
        }
    }

    if (exif_ifd_offset != 0) {
        std::vector<TiffEntry> exif_entries;
        if (parse_ifd_entries(tiff, little_endian, exif_ifd_offset, exif_entries)) {
            if (const TiffEntry* original = find_entry(exif_entries, kTagDateTimeOriginal)) {
                if (const auto parsed = read_ascii_value(tiff, *original)) {
                    metadata.capture_date = normalize_exif_date_value(*parsed);
                }
            }
            if (!metadata.capture_date.has_value()) {
                if (const TiffEntry* created = find_entry(exif_entries, kTagCreateDate)) {
                    if (const auto parsed = read_ascii_value(tiff, *created)) {
                        metadata.capture_date = normalize_exif_date_value(*parsed);
                    }
                }
            }
        }
    }

    uint32_t gps_ifd_offset = 0;
    if (const TiffEntry* gps_ptr = find_entry(ifd0_entries, kTagGpsIfd)) {
        if (gps_ptr->count == 1 && gps_ptr->type == 4) {
            gps_ifd_offset = gps_ptr->value_or_offset;
        }
    }

    if (gps_ifd_offset != 0) {
        std::vector<TiffEntry> gps_entries;
        if (parse_ifd_entries(tiff, little_endian, gps_ifd_offset, gps_entries)) {
            char lat_ref = '\0';
            char lon_ref = '\0';
            std::optional<std::array<double, 3>> lat_dms;
            std::optional<std::array<double, 3>> lon_dms;

            if (const TiffEntry* lat_ref_entry = find_entry(gps_entries, kTagGpsLatitudeRef)) {
                if (const auto text = read_ascii_value(tiff, *lat_ref_entry); text && !text->empty()) {
                    lat_ref = static_cast<char>(std::toupper(static_cast<unsigned char>((*text)[0])));
                }
            }

            if (const TiffEntry* lon_ref_entry = find_entry(gps_entries, kTagGpsLongitudeRef)) {
                if (const auto text = read_ascii_value(tiff, *lon_ref_entry); text && !text->empty()) {
                    lon_ref = static_cast<char>(std::toupper(static_cast<unsigned char>((*text)[0])));
                }
            }

            if (const TiffEntry* lat_entry = find_entry(gps_entries, kTagGpsLatitude)) {
                lat_dms = read_rational_triplet(tiff, *lat_entry, little_endian);
            }

            if (const TiffEntry* lon_entry = find_entry(gps_entries, kTagGpsLongitude)) {
                lon_dms = read_rational_triplet(tiff, *lon_entry, little_endian);
            }

            if (lat_dms && lon_dms && (lat_ref == 'N' || lat_ref == 'S') && (lon_ref == 'E' || lon_ref == 'W')) {
                const auto latitude = decode_gps_coordinate(*lat_dms, lat_ref);
                const auto longitude = decode_gps_coordinate(*lon_dms, lon_ref);
                if (latitude && longitude &&
                    std::abs(*latitude) <= 90.0 && std::abs(*longitude) <= 180.0) {
                    metadata.latitude = *latitude;
                    metadata.longitude = *longitude;
                }
            }
        }
    }

    return metadata;
}

std::string shell_quote_argument(const std::string& raw)
{
#if defined(_WIN32)
    std::string output;
    output.reserve(raw.size() + 2);
    output.push_back('"');
    for (char ch : raw) {
        if (ch == '"') {
            output += "\\\"";
        } else {
            output.push_back(ch);
        }
    }
    output.push_back('"');
    return output;
#else
    std::string output;
    output.reserve(raw.size() + 2);
    output.push_back('\'');
    for (char ch : raw) {
        if (ch == '\'') {
            output += "'\"'\"'";
        } else {
            output.push_back(ch);
        }
    }
    output.push_back('\'');
    return output;
#endif
}

std::optional<std::string> run_command_capture_output(const std::string& command)
{
#if defined(_WIN32)
    FILE* pipe = _popen(command.c_str(), "rb");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (!pipe) {
        return std::nullopt;
    }

    std::array<char, 4096> buffer{};
    std::string output;
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output.append(buffer.data());
        if (output.size() > kHeifProbeOutputLimit) {
            break;
        }
    }

#if defined(_WIN32)
    const int exit_code = _pclose(pipe);
#else
    const int exit_code = pclose(pipe);
#endif

    if (exit_code != 0) {
        return std::nullopt;
    }
    return output;
}

std::optional<std::string> json_string_field(const Json::Value& object, const char* key)
{
    if (!object.isObject() || !key || !object.isMember(key) || !object[key].isString()) {
        return std::nullopt;
    }
    const std::string value = trim_copy(object[key].asString());
    if (value.empty()) {
        return std::nullopt;
    }
    return value;
}

std::optional<double> json_number_field(const Json::Value& object, const char* key)
{
    if (!object.isObject() || !key || !object.isMember(key)) {
        return std::nullopt;
    }

    const Json::Value& value = object[key];
    if (value.isDouble() || value.isInt() || value.isUInt() || value.isInt64() || value.isUInt64()) {
        return value.asDouble();
    }

    if (value.isString()) {
        const std::string text = trim_copy(value.asString());
        if (text.empty()) {
            return std::nullopt;
        }
        try {
            size_t consumed = 0;
            const double numeric = std::stod(text, &consumed);
            if (consumed == text.size()) {
                return numeric;
            }
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

ParsedExifMetadata extract_heif_metadata_with_exiftool(const std::filesystem::path& image_path)
{
    ParsedExifMetadata metadata;

    const std::string path_utf8 = Utils::path_to_utf8(image_path);
    if (path_utf8.empty()) {
        return metadata;
    }

    std::ostringstream command;
#if defined(_WIN32)
    command << "exiftool -j -n -DateTimeOriginal -CreateDate -DateTime -GPSLatitude -GPSLongitude -- "
            << shell_quote_argument(path_utf8) << " 2>NUL";
#else
    command << "exiftool -j -n -DateTimeOriginal -CreateDate -DateTime -GPSLatitude -GPSLongitude -- "
            << shell_quote_argument(path_utf8) << " 2>/dev/null";
#endif

    const auto output = run_command_capture_output(command.str());
    if (!output || output->empty()) {
        return metadata;
    }

    Json::CharReaderBuilder reader_builder;
    Json::Value root;
    std::string errors;
    std::istringstream stream(*output);
    if (!Json::parseFromStream(reader_builder, stream, &root, &errors) ||
        !root.isArray() || root.empty() || !root[0].isObject()) {
        return metadata;
    }

    const Json::Value& item = root[0];

    if (const auto date = json_string_field(item, "DateTimeOriginal")) {
        metadata.capture_date = normalize_exif_date_value(*date);
    }
    if (!metadata.capture_date.has_value()) {
        if (const auto date = json_string_field(item, "CreateDate")) {
            metadata.capture_date = normalize_exif_date_value(*date);
        }
    }
    if (!metadata.capture_date.has_value()) {
        if (const auto date = json_string_field(item, "DateTime")) {
            metadata.capture_date = normalize_exif_date_value(*date);
        }
    }

    const auto latitude = json_number_field(item, "GPSLatitude");
    const auto longitude = json_number_field(item, "GPSLongitude");
    if (latitude && longitude &&
        std::isfinite(*latitude) && std::isfinite(*longitude) &&
        std::abs(*latitude) <= 90.0 && std::abs(*longitude) <= 180.0) {
        metadata.latitude = *latitude;
        metadata.longitude = *longitude;
    }

    return metadata;
}

std::optional<std::string> pick_place_name(const Json::Value& root)
{
    static const std::array<const char*, 9> kAddressKeys = {
        "city", "town", "village", "hamlet", "municipality", "suburb", "county", "state", "country"
    };

    if (root.isMember("address") && root["address"].isObject()) {
        const auto& address = root["address"];
        for (const char* key : kAddressKeys) {
            if (address.isMember(key) && address[key].isString()) {
                const std::string value = trim_copy(address[key].asString());
                if (!value.empty()) {
                    return value;
                }
            }
        }
    }

    if (root.isMember("name") && root["name"].isString()) {
        const std::string value = trim_copy(root["name"].asString());
        if (!value.empty()) {
            return value;
        }
    }

    if (root.isMember("display_name") && root["display_name"].isString()) {
        std::string display = root["display_name"].asString();
        const auto comma = display.find(',');
        if (comma != std::string::npos) {
            display = display.substr(0, comma);
        }
        display = trim_copy(display);
        if (!display.empty()) {
            return display;
        }
    }

    return std::nullopt;
}

} // namespace

ImageRenameMetadataService::ImageRenameMetadataService(std::string config_dir)
    : config_dir_(std::move(config_dir))
{
    open_cache_db();
}

ImageRenameMetadataService::~ImageRenameMetadataService()
{
    if (cache_db_) {
        sqlite3_close(cache_db_);
        cache_db_ = nullptr;
    }
}

bool ImageRenameMetadataService::open_cache_db()
{
    if (cache_db_) {
        return true;
    }

    if (config_dir_.empty()) {
        return false;
    }

    std::error_code ec;
    const auto config_path = Utils::utf8_to_path(config_dir_);
    std::filesystem::create_directories(config_path, ec);
    if (ec) {
        return false;
    }

    const auto db_path = config_path / "image_place_cache.db";
    if (sqlite3_open(db_path.string().c_str(), &cache_db_) != SQLITE_OK) {
        if (cache_db_) {
            sqlite3_close(cache_db_);
            cache_db_ = nullptr;
        }
        return false;
    }

    const char* create_sql = R"(
        CREATE TABLE IF NOT EXISTS reverse_geocode_cache (
            latitude_key TEXT NOT NULL,
            longitude_key TEXT NOT NULL,
            place_slug TEXT,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY(latitude_key, longitude_key)
        );
    )";

    char* error = nullptr;
    if (sqlite3_exec(cache_db_, create_sql, nullptr, nullptr, &error) != SQLITE_OK) {
        if (error) {
            sqlite3_free(error);
        }
        sqlite3_close(cache_db_);
        cache_db_ = nullptr;
        return false;
    }

    return true;
}

ImageRenameMetadataService::ExifMetadata ImageRenameMetadataService::extract_exif_metadata(
    const std::filesystem::path& image_path) const
{
    ExifMetadata metadata;

    const std::string extension = file_extension_lower(image_path);
    const bool is_heif = has_extension(extension, {".heic", ".heif", ".hif"});

    std::optional<std::vector<uint8_t>> exif_tiff;
    if (has_extension(extension, {".jpg", ".jpeg"})) {
        exif_tiff = read_jpeg_exif_payload(image_path);
    } else if (has_extension(extension, {".tif", ".tiff"})) {
        exif_tiff = read_tiff_exif_payload(image_path);
    } else if (has_extension(extension, {".png"})) {
        exif_tiff = read_png_exif_payload(image_path);
    }

    if (!exif_tiff) {
        exif_tiff = read_jpeg_exif_payload(image_path);
    }
    if (!exif_tiff) {
        exif_tiff = read_tiff_exif_payload(image_path);
    }
    if (!exif_tiff) {
        exif_tiff = read_png_exif_payload(image_path);
    }

    if (exif_tiff) {
        const ParsedExifMetadata parsed = parse_tiff_metadata(*exif_tiff);
        metadata.capture_date = parsed.capture_date;
        metadata.latitude = parsed.latitude;
        metadata.longitude = parsed.longitude;
        return metadata;
    }

    if (is_heif) {
        const ParsedExifMetadata parsed = extract_heif_metadata_with_exiftool(image_path);
        metadata.capture_date = parsed.capture_date;
        metadata.latitude = parsed.latitude;
        metadata.longitude = parsed.longitude;
    }

    return metadata;
}

ImageRenameMetadataService::CacheLookup ImageRenameMetadataService::lookup_cache(
    const std::string& lat_key,
    const std::string& lon_key) const
{
    CacheLookup lookup;
    if (!cache_db_) {
        return lookup;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* query_sql =
        "SELECT place_slug FROM reverse_geocode_cache WHERE latitude_key = ? AND longitude_key = ?;";

    if (sqlite3_prepare_v2(cache_db_, query_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return lookup;
    }

    sqlite3_bind_text(stmt, 1, lat_key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, lon_key.c_str(), -1, SQLITE_TRANSIENT);

    const int step = sqlite3_step(stmt);
    if (step == SQLITE_ROW) {
        lookup.found = true;
        if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
            const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (text && *text != '\0') {
                lookup.place = text;
            }
        }
    }

    sqlite3_finalize(stmt);
    return lookup;
}

void ImageRenameMetadataService::upsert_cache(const std::string& lat_key,
                                              const std::string& lon_key,
                                              const std::optional<std::string>& place) const
{
    if (!cache_db_) {
        return;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* upsert_sql = R"(
        INSERT INTO reverse_geocode_cache (latitude_key, longitude_key, place_slug, updated_at)
        VALUES (?, ?, ?, CURRENT_TIMESTAMP)
        ON CONFLICT(latitude_key, longitude_key)
        DO UPDATE SET place_slug = excluded.place_slug,
                      updated_at = CURRENT_TIMESTAMP;
    )";

    if (sqlite3_prepare_v2(cache_db_, upsert_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }

    sqlite3_bind_text(stmt, 1, lat_key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, lon_key.c_str(), -1, SQLITE_TRANSIENT);

    if (place.has_value() && !place->empty()) {
        sqlite3_bind_text(stmt, 3, place->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, 3);
    }

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

ImageRenameMetadataService::ReverseGeocodeResult ImageRenameMetadataService::reverse_geocode(
    double latitude,
    double longitude)
{
    ReverseGeocodeResult result;

    CURL* curl = curl_easy_init();
    if (!curl) {
        return result;
    }

    std::ostringstream url_builder;
    const char* endpoint = std::getenv("AI_FILE_SORTER_NOMINATIM_URL");
    url_builder << ((endpoint && endpoint[0] != '\0') ? endpoint : kDefaultNominatimUrl)
                << "?format=jsonv2&addressdetails=1&zoom=10&lat="
                << std::fixed << std::setprecision(7) << latitude
                << "&lon=" << std::fixed << std::setprecision(7) << longitude;

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url_builder.str().c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "AIFileSorter/1.7 (reverse-geocoder)");
    configure_tls(curl);

    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/json");
    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    const CURLcode code = curl_easy_perform(curl);
    long http_status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status);

    if (headers) {
        curl_slist_free_all(headers);
    }
    curl_easy_cleanup(curl);

    if (code != CURLE_OK || http_status != 200) {
        return result;
    }

    Json::CharReaderBuilder reader_builder;
    Json::Value root;
    std::string errors;
    std::istringstream stream(response);
    if (!Json::parseFromStream(reader_builder, stream, &root, &errors)) {
        return result;
    }

    const auto place_name = pick_place_name(root);
    result.success = true;
    if (place_name.has_value()) {
        const std::string place_slug = slugify(*place_name);
        if (!place_slug.empty()) {
            result.place = place_slug;
        }
    }

    return result;
}

std::optional<std::string> ImageRenameMetadataService::resolve_place_prefix(double latitude,
                                                                             double longitude)
{
    // Requirement: no network available -> no place prefix.
    if (!network_available()) {
        return std::nullopt;
    }

    const std::string lat_key = format_coord_key(latitude);
    const std::string lon_key = format_coord_key(longitude);

    const CacheLookup cached = lookup_cache(lat_key, lon_key);
    if (cached.found) {
        return cached.place;
    }

    const auto now = std::chrono::steady_clock::now();
    if (last_geocode_request_.time_since_epoch().count() != 0) {
        const auto elapsed = now - last_geocode_request_;
        if (elapsed < kMinReverseInterval) {
            std::this_thread::sleep_for(kMinReverseInterval - elapsed);
        }
    }

    const auto geocode = reverse_geocode(latitude, longitude);
    last_geocode_request_ = std::chrono::steady_clock::now();

    if (geocode.success) {
        upsert_cache(lat_key, lon_key, geocode.place);
    }

    return geocode.place;
}

std::string ImageRenameMetadataService::enrich_suggested_name(
    const std::filesystem::path& image_path,
    const std::string& suggested_name)
{
    if (suggested_name.empty()) {
        return suggested_name;
    }

    const ExifMetadata metadata = extract_exif_metadata(image_path);

    std::optional<std::string> place_prefix;
    if (metadata.latitude.has_value() && metadata.longitude.has_value()) {
        place_prefix = resolve_place_prefix(*metadata.latitude, *metadata.longitude);
    }

    return apply_prefix_to_filename(suggested_name, metadata.capture_date, place_prefix);
}

std::string ImageRenameMetadataService::apply_prefix_to_filename(
    const std::string& suggested_name,
    const std::optional<std::string>& date_prefix,
    const std::optional<std::string>& place_prefix)
{
    if (suggested_name.empty()) {
        return suggested_name;
    }

    std::vector<std::string> prefix_parts;
    if (date_prefix.has_value() && !date_prefix->empty()) {
        prefix_parts.push_back(*date_prefix);
    }
    if (place_prefix.has_value() && !place_prefix->empty()) {
        const std::string place_slug = slugify(*place_prefix);
        if (!place_slug.empty()) {
            prefix_parts.push_back(place_slug);
        }
    }

    if (prefix_parts.empty()) {
        return suggested_name;
    }

    const auto suggested_path = Utils::utf8_to_path(suggested_name);
    std::string stem = Utils::path_to_utf8(suggested_path.stem());
    std::string ext = Utils::path_to_utf8(suggested_path.extension());

    if (stem.empty()) {
        stem = suggested_name;
        ext.clear();
    }

    std::ostringstream prefix_builder;
    for (size_t index = 0; index < prefix_parts.size(); ++index) {
        if (index > 0) {
            prefix_builder << '_';
        }
        prefix_builder << prefix_parts[index];
    }
    const std::string prefix = prefix_builder.str();
    if (prefix.empty()) {
        return suggested_name;
    }

    if (stem == prefix || stem.rfind(prefix + "_", 0) == 0) {
        return suggested_name;
    }

    const std::string prefixed_stem = prefix + "_" + stem;
    return ext.empty() ? prefixed_stem : prefixed_stem + ext;
}

std::string ImageRenameMetadataService::slugify(const std::string& value)
{
    std::string output;
    output.reserve(value.size());

    bool last_sep = true;
    for (unsigned char ch : value) {
        if (std::isalnum(ch)) {
            output.push_back(static_cast<char>(std::tolower(ch)));
            last_sep = false;
            continue;
        }

        if (!last_sep && !output.empty()) {
            output.push_back('_');
            last_sep = true;
        }
    }

    while (!output.empty() && output.back() == '_') {
        output.pop_back();
    }

    return output;
}

std::optional<std::string> ImageRenameMetadataService::normalize_exif_date(const std::string& value)
{
    return normalize_exif_date_value(value);
}

std::string ImageRenameMetadataService::format_coord_key(double value)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(4) << value;
    return out.str();
}

bool ImageRenameMetadataService::network_available()
{
    if (!network_checked_) {
        network_available_ = Utils::is_network_available();
        network_checked_ = true;
    }
    return network_available_;
}
