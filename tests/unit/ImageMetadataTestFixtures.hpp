#pragma once

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

/** @brief Synthetic TIFF EXIF fixtures containing dates and arbitrary test coordinates. */
namespace ImageMetadataTestFixtures {
/** @brief Append a little-endian integer. @param data Output bytes. @param value Integer value. */
inline void append_u16(std::vector<std::uint8_t>& data, std::uint16_t value)
{
    data.push_back(value & 0xff);
    data.push_back((value >> 8) & 0xff);
}
/** @brief Append a little-endian integer. @param data Output bytes. @param value Integer value. */
inline void append_u32(std::vector<std::uint8_t>& data, std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8) data.push_back((value >> shift) & 0xff);
}
/**
 * @brief Append a TIFF directory entry.
 * @param data Output bytes. @param tag EXIF tag. @param type TIFF type.
 * @param count Value count. @param value Inline value or payload offset.
 */
inline void entry(std::vector<std::uint8_t>& data, std::uint16_t tag,
                  std::uint16_t type, std::uint32_t count, std::uint32_t value)
{
    append_u16(data, tag); append_u16(data, type);
    append_u32(data, count); append_u32(data, value);
}
/** @brief Append rational degrees/minutes/seconds. @param data Output bytes. @param value Coordinate. */
inline void coordinate(std::vector<std::uint8_t>& data, double value)
{
    constexpr std::uint32_t kSecondsDenominator = 10'000'000;
    value = std::abs(value);
    const auto degrees = static_cast<std::uint32_t>(value);
    const auto minutes = static_cast<std::uint32_t>((value - degrees) * 60);
    const double seconds = ((value - degrees) * 60 - minutes) * 60;
    append_u32(data, degrees); append_u32(data, 1);
    append_u32(data, minutes); append_u32(data, 1);
    append_u32(data, static_cast<std::uint32_t>(std::llround(seconds * kSecondsDenominator)));
    append_u32(data, kSecondsDenominator);
}
/**
 * @brief Build TIFF EXIF carrying a fixed capture date and synthetic GPS coordinates.
 * @param latitude Test latitude. @param longitude Test longitude.
 * @return TIFF metadata bytes.
 */
inline std::vector<std::uint8_t> gps_tiff(double latitude, double longitude)
{
    constexpr std::uint32_t kDirectoryEntryBytes = 12;
    constexpr std::uint32_t kDateOffset = 8 + 2 + 2 * kDirectoryEntryBytes + 4;
    const std::string date = std::string("2020:01:02 03:04:05") + '\0';
    const auto gps_offset = kDateOffset + static_cast<std::uint32_t>(date.size());
    const auto coordinate_offset = gps_offset + 2 + 4 * kDirectoryEntryBytes + 4;
    constexpr std::uint32_t kCoordinateBytes = 3 * 2 * sizeof(std::uint32_t);
    std::vector<std::uint8_t> data{'I', 'I'};
    append_u16(data, 42); append_u32(data, 8);
    append_u16(data, 2);
    entry(data, 0x0132, 2, date.size(), kDateOffset);
    entry(data, 0x8825, 4, 1, gps_offset);
    append_u32(data, 0);
    data.insert(data.end(), date.begin(), date.end());
    append_u16(data, 4);
    entry(data, 1, 2, 2, latitude < 0 ? 'S' : 'N');
    entry(data, 2, 5, 3, coordinate_offset);
    entry(data, 3, 2, 2, longitude < 0 ? 'W' : 'E');
    entry(data, 4, 5, 3, coordinate_offset + kCoordinateBytes);
    append_u32(data, 0);
    coordinate(data, latitude); coordinate(data, longitude);
    return data;
}
/** @brief Write fixture bytes. @param path Destination. @param data Binary contents. */
inline void write_bytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& data)
{
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!output) throw std::runtime_error("Cannot write synthetic GPS fixture");
}
/**
 * @brief Write a TIFF GPS fixture.
 * @param path Destination. @param latitude Test latitude. @param longitude Test longitude.
 */
inline void write_gps_tiff(const std::filesystem::path& path, double latitude, double longitude)
{
    write_bytes(path, gps_tiff(latitude, longitude));
}
/**
 * @brief Write a metadata-only PNG for cached/unavailable image workflow paths.
 * @param path Destination. @param latitude Test latitude. @param longitude Test longitude.
 * @note No image decoder is used in these paths; the EXIF parser does not validate PNG CRCs.
 */
inline void write_gps_png(const std::filesystem::path& path, double latitude, double longitude)
{
    const auto tiff = gps_tiff(latitude, longitude);
    std::vector<std::uint8_t> png{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    const auto size = static_cast<std::uint32_t>(tiff.size());
    for (int shift = 24; shift >= 0; shift -= 8) png.push_back((size >> shift) & 0xff);
    for (auto value : {'e', 'X', 'I', 'f'}) png.push_back(value);
    png.insert(png.end(), tiff.begin(), tiff.end());
    append_u32(png, 0); // Unchecked eXIf CRC.
    append_u32(png, 0); // Empty IEND length.
    for (auto value : {'I', 'E', 'N', 'D'}) png.push_back(value);
    append_u32(png, 0); // Unchecked IEND CRC.
    write_bytes(path, png);
}
} // namespace ImageMetadataTestFixtures
