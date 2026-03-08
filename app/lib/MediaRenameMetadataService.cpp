#include "MediaRenameMetadataService.hpp"

#include "Utils.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#ifdef AI_FILE_SORTER_USE_MEDIAINFOLIB
#if defined(__APPLE__) && !defined(UNICODE) && !defined(_UNICODE)
// Homebrew's MediaInfoLib is built with the wide-character API enabled.
#define UNICODE
#define _UNICODE
#endif
#if defined(_WIN32)
#include <MediaInfoDLL/MediaInfoDLL_Static.h>
namespace MediaInfoCompat = MediaInfoDLL;
#else
#include <MediaInfo/MediaInfo.h>
namespace MediaInfoCompat = MediaInfoLib;
#endif
#if defined(UNICODE) || defined(_UNICODE)
#include <QString>
#endif
#endif

namespace {

std::string trim_copy(std::string value)
{
    const char* whitespace = " \t\n\r\f\v";
    const auto start = value.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return std::string();
    }
    const auto end = value.find_last_not_of(whitespace);
    return value.substr(start, end - start + 1);
}

std::string to_lower_copy(std::string value)
{
    std::transform(value.begin(),
                   value.end(),
                   value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

const std::unordered_set<std::string>& audio_extensions()
{
    static const std::unordered_set<std::string> extensions = {
        ".aac", ".aif", ".aiff", ".alac", ".ape", ".flac", ".m4a", ".mp3",
        ".ogg", ".oga", ".opus", ".wav", ".wma"
    };
    return extensions;
}

const std::unordered_set<std::string>& video_extensions()
{
    static const std::unordered_set<std::string> extensions = {
        ".3gp", ".avi", ".flv", ".m4v", ".mkv", ".mov", ".mp4", ".mpeg", ".mpg",
        ".mts", ".m2ts", ".ts", ".webm", ".wmv"
    };
    return extensions;
}

bool is_supported_audio(const std::filesystem::path& path)
{
    if (!path.has_extension()) {
        return false;
    }
    const std::string ext = to_lower_copy(Utils::path_to_utf8(path.extension()));
    return audio_extensions().contains(ext);
}

bool is_supported_video(const std::filesystem::path& path)
{
    if (!path.has_extension()) {
        return false;
    }
    const std::string ext = to_lower_copy(Utils::path_to_utf8(path.extension()));
    return video_extensions().contains(ext);
}

bool has_metadata_parts(const MediaRenameMetadataService::MetadataFields& metadata)
{
    return metadata.year.has_value() ||
           metadata.artist.has_value() ||
           metadata.album.has_value() ||
           metadata.title.has_value();
}

#ifndef AI_FILE_SORTER_USE_MEDIAINFOLIB
constexpr std::size_t kMaxId3TagSizeBytes = 2U * 1024U * 1024U;

bool read_exact(std::istream& input, char* destination, std::size_t size)
{
    if (size == 0) {
        return true;
    }
    input.read(destination, static_cast<std::streamsize>(size));
    return input.gcount() == static_cast<std::streamsize>(size);
}

std::uint32_t read_u32_be(const std::uint8_t* bytes)
{
    return (static_cast<std::uint32_t>(bytes[0]) << 24) |
           (static_cast<std::uint32_t>(bytes[1]) << 16) |
           (static_cast<std::uint32_t>(bytes[2]) << 8) |
           static_cast<std::uint32_t>(bytes[3]);
}

std::uint32_t read_u32_le(const std::uint8_t* bytes)
{
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

std::uint32_t read_u24_be(const std::uint8_t* bytes)
{
    return (static_cast<std::uint32_t>(bytes[0]) << 16) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           static_cast<std::uint32_t>(bytes[2]);
}

std::uint64_t read_u64_be(const std::uint8_t* bytes)
{
    return (static_cast<std::uint64_t>(bytes[0]) << 56) |
           (static_cast<std::uint64_t>(bytes[1]) << 48) |
           (static_cast<std::uint64_t>(bytes[2]) << 40) |
           (static_cast<std::uint64_t>(bytes[3]) << 32) |
           (static_cast<std::uint64_t>(bytes[4]) << 24) |
           (static_cast<std::uint64_t>(bytes[5]) << 16) |
           (static_cast<std::uint64_t>(bytes[6]) << 8) |
           static_cast<std::uint64_t>(bytes[7]);
}

std::uint32_t read_synchsafe_u32(const std::uint8_t* bytes)
{
    return (static_cast<std::uint32_t>(bytes[0] & 0x7F) << 21) |
           (static_cast<std::uint32_t>(bytes[1] & 0x7F) << 14) |
           (static_cast<std::uint32_t>(bytes[2] & 0x7F) << 7) |
           static_cast<std::uint32_t>(bytes[3] & 0x7F);
}

std::string sanitize_metadata_value(std::string value)
{
    std::string sanitized;
    sanitized.reserve(value.size());

    for (unsigned char ch : value) {
        if (ch == '\0') {
            continue;
        }
        if (std::iscntrl(ch)) {
            sanitized.push_back(' ');
            continue;
        }
        sanitized.push_back(static_cast<char>(ch));
    }

    return trim_copy(sanitized);
}

void assign_if_missing(std::optional<std::string>& target, const std::optional<std::string>& candidate)
{
    if (!target.has_value() && candidate.has_value() && !candidate->empty()) {
        target = candidate;
    }
}

void assign_vorbis_comment_field(const std::string& key,
                                 const std::string& value,
                                 MediaRenameMetadataService::MetadataFields& metadata)
{
    if (value.empty()) {
        return;
    }

    const std::string lowered_key = to_lower_copy(key);
    if (lowered_key == "title") {
        assign_if_missing(metadata.title, sanitize_metadata_value(value));
        return;
    }
    if (lowered_key == "artist" || lowered_key == "albumartist" || lowered_key == "album artist") {
        assign_if_missing(metadata.artist, sanitize_metadata_value(value));
        return;
    }
    if (lowered_key == "album") {
        assign_if_missing(metadata.album, sanitize_metadata_value(value));
        return;
    }
    if (lowered_key == "date" || lowered_key == "year" || lowered_key == "originaldate") {
        assign_if_missing(metadata.year, sanitize_metadata_value(value));
    }
}

std::optional<std::string> decode_id3_text_frame(const std::uint8_t* bytes, std::size_t size)
{
    if (bytes == nullptr || size <= 1) {
        return std::nullopt;
    }

    const std::uint8_t encoding = bytes[0];
    const std::uint8_t* payload = bytes + 1;
    const std::size_t payload_size = size - 1;

    std::string decoded;
    if (encoding == 0 || encoding == 3) {
        decoded.assign(reinterpret_cast<const char*>(payload), payload_size);
    } else if (encoding == 1 || encoding == 2) {
        bool little_endian = (encoding == 1);
        std::size_t offset = 0;
        if (encoding == 1 && payload_size >= 2) {
            if (payload[0] == 0xFF && payload[1] == 0xFE) {
                little_endian = true;
                offset = 2;
            } else if (payload[0] == 0xFE && payload[1] == 0xFF) {
                little_endian = false;
                offset = 2;
            }
        }

        decoded.reserve(payload_size / 2);
        for (std::size_t index = offset; index + 1 < payload_size; index += 2) {
            const std::uint16_t code_unit = little_endian
                                                ? static_cast<std::uint16_t>(payload[index]) |
                                                      (static_cast<std::uint16_t>(payload[index + 1]) << 8)
                                                : (static_cast<std::uint16_t>(payload[index]) << 8) |
                                                      static_cast<std::uint16_t>(payload[index + 1]);
            if (code_unit == 0) {
                break;
            }
            if (code_unit <= 0x7F) {
                decoded.push_back(static_cast<char>(code_unit));
            } else {
                decoded.push_back(' ');
            }
        }
    } else {
        return std::nullopt;
    }

    if (const auto null_index = decoded.find('\0'); null_index != std::string::npos) {
        decoded.resize(null_index);
    }

    decoded = sanitize_metadata_value(decoded);
    if (decoded.empty()) {
        return std::nullopt;
    }
    return decoded;
}

std::vector<std::uint8_t> remove_unsynchronization(const std::vector<std::uint8_t>& input)
{
    std::vector<std::uint8_t> decoded;
    decoded.reserve(input.size());
    for (std::size_t index = 0; index < input.size(); ++index) {
        const std::uint8_t byte = input[index];
        if (byte == 0xFF && index + 1 < input.size() && input[index + 1] == 0x00) {
            decoded.push_back(0xFF);
            ++index;
            continue;
        }
        decoded.push_back(byte);
    }
    return decoded;
}

bool parse_vorbis_comment_payload(const std::uint8_t* bytes,
                                  std::size_t size,
                                  MediaRenameMetadataService::MetadataFields& metadata)
{
    if (bytes == nullptr || size < 8) {
        return false;
    }

    std::size_t offset = 0;
    const auto read_u32 = [&](std::uint32_t& value) -> bool {
        if (offset + 4 > size) {
            return false;
        }
        value = read_u32_le(bytes + offset);
        offset += 4;
        return true;
    };

    std::uint32_t vendor_length = 0;
    if (!read_u32(vendor_length) || offset + vendor_length > size) {
        return false;
    }
    offset += vendor_length;

    std::uint32_t comment_count = 0;
    if (!read_u32(comment_count)) {
        return false;
    }

    for (std::uint32_t index = 0; index < comment_count; ++index) {
        std::uint32_t comment_length = 0;
        if (!read_u32(comment_length) || offset + comment_length > size) {
            return false;
        }

        const std::string entry(reinterpret_cast<const char*>(bytes + offset), comment_length);
        offset += comment_length;

        const auto separator_index = entry.find('=');
        if (separator_index == std::string::npos || separator_index == 0 ||
            separator_index + 1 >= entry.size()) {
            continue;
        }

        const std::string key = trim_copy(entry.substr(0, separator_index));
        const std::string value = entry.substr(separator_index + 1);
        assign_vorbis_comment_field(key, value, metadata);
    }

    return has_metadata_parts(metadata);
}

bool parse_id3v2_frames(const std::vector<std::uint8_t>& tag_data,
                        int id3_major_version,
                        std::uint8_t id3_flags,
                        MediaRenameMetadataService::MetadataFields& metadata)
{
    if (id3_major_version < 2 || id3_major_version > 4 || tag_data.empty()) {
        return false;
    }

    std::vector<std::uint8_t> data = tag_data;
    if ((id3_flags & 0x80) != 0) {
        data = remove_unsynchronization(data);
    }

    std::size_t offset = 0;
    if ((id3_flags & 0x40) != 0) {
        if (data.size() < 4) {
            return false;
        }

        if (id3_major_version == 3) {
            const std::size_t ext_size = read_u32_be(data.data());
            if (ext_size + 4 > data.size()) {
                return false;
            }
            offset = ext_size + 4;
        } else if (id3_major_version == 4) {
            const std::size_t ext_size = read_synchsafe_u32(data.data());
            if (ext_size > data.size()) {
                return false;
            }
            offset = ext_size;
        }
    }

    auto apply_frame = [&](const std::string& frame_id, const std::optional<std::string>& value) {
        if (!value.has_value()) {
            return;
        }

        if (frame_id == "TIT2" || frame_id == "TT2") {
            assign_if_missing(metadata.title, *value);
            return;
        }
        if (frame_id == "TPE1" || frame_id == "TP1" ||
            frame_id == "TPE2" || frame_id == "TP2") {
            assign_if_missing(metadata.artist, *value);
            return;
        }
        if (frame_id == "TALB" || frame_id == "TAL") {
            assign_if_missing(metadata.album, *value);
            return;
        }
        if (frame_id == "TDRC" || frame_id == "TYER" || frame_id == "TYE" ||
            frame_id == "TDOR" || frame_id == "TDRL") {
            assign_if_missing(metadata.year, *value);
        }
    };

    while (offset < data.size()) {
        std::string frame_id;
        std::size_t frame_size = 0;
        std::size_t frame_header_size = 0;

        if (id3_major_version == 2) {
            if (offset + 6 > data.size()) {
                break;
            }
            const std::uint8_t* frame_header = data.data() + offset;
            if (frame_header[0] == 0 && frame_header[1] == 0 && frame_header[2] == 0) {
                break;
            }
            frame_id.assign(reinterpret_cast<const char*>(frame_header), 3);
            frame_size = read_u24_be(frame_header + 3);
            frame_header_size = 6;
        } else {
            if (offset + 10 > data.size()) {
                break;
            }
            const std::uint8_t* frame_header = data.data() + offset;
            if (frame_header[0] == 0 && frame_header[1] == 0 &&
                frame_header[2] == 0 && frame_header[3] == 0) {
                break;
            }
            frame_id.assign(reinterpret_cast<const char*>(frame_header), 4);
            frame_size = id3_major_version == 4
                             ? read_synchsafe_u32(frame_header + 4)
                             : read_u32_be(frame_header + 4);
            frame_header_size = 10;
        }

        if (frame_size == 0 || offset + frame_header_size + frame_size > data.size()) {
            break;
        }

        const std::uint8_t* frame_body = data.data() + offset + frame_header_size;
        apply_frame(frame_id, decode_id3_text_frame(frame_body, frame_size));
        offset += frame_header_size + frame_size;
    }

    return has_metadata_parts(metadata);
}

bool parse_id3v2_from_file(const std::filesystem::path& media_path,
                           MediaRenameMetadataService::MetadataFields& metadata)
{
    std::ifstream input(media_path, std::ios::binary);
    if (!input) {
        return false;
    }

    std::uint8_t header[10] = {};
    if (!read_exact(input, reinterpret_cast<char*>(header), sizeof(header))) {
        return false;
    }

    if (std::memcmp(header, "ID3", 3) != 0) {
        return false;
    }

    const int id3_major_version = static_cast<int>(header[3]);
    if (id3_major_version < 2 || id3_major_version > 4) {
        return false;
    }

    const std::uint32_t tag_size = read_synchsafe_u32(header + 6);
    if (tag_size == 0 || tag_size > kMaxId3TagSizeBytes) {
        return false;
    }

    std::vector<std::uint8_t> tag_data(tag_size);
    if (!read_exact(input, reinterpret_cast<char*>(tag_data.data()), tag_data.size())) {
        return false;
    }

    return parse_id3v2_frames(tag_data, id3_major_version, header[5], metadata);
}

bool parse_id3v1_from_file(const std::filesystem::path& media_path,
                           MediaRenameMetadataService::MetadataFields& metadata)
{
    std::ifstream input(media_path, std::ios::binary);
    if (!input) {
        return false;
    }

    input.seekg(0, std::ios::end);
    const std::streamoff file_size = input.tellg();
    if (file_size < 128) {
        return false;
    }
    input.seekg(-128, std::ios::end);

    std::array<char, 128> tag{};
    if (!read_exact(input, tag.data(), tag.size())) {
        return false;
    }
    if (std::memcmp(tag.data(), "TAG", 3) != 0) {
        return false;
    }

    const auto read_field = [&](std::size_t offset, std::size_t length) -> std::optional<std::string> {
        std::string value(tag.data() + offset, length);
        while (!value.empty() && (value.back() == '\0' || std::isspace(static_cast<unsigned char>(value.back())))) {
            value.pop_back();
        }
        value = sanitize_metadata_value(value);
        if (value.empty()) {
            return std::nullopt;
        }
        return value;
    };

    assign_if_missing(metadata.title, read_field(3, 30));
    assign_if_missing(metadata.artist, read_field(33, 30));
    assign_if_missing(metadata.album, read_field(63, 30));
    assign_if_missing(metadata.year, read_field(93, 4));

    return has_metadata_parts(metadata);
}

bool parse_flac_vorbis_comments(const std::filesystem::path& media_path,
                                MediaRenameMetadataService::MetadataFields& metadata)
{
    std::ifstream input(media_path, std::ios::binary);
    if (!input) {
        return false;
    }

    std::array<char, 4> signature{};
    if (!read_exact(input, signature.data(), signature.size()) ||
        std::memcmp(signature.data(), "fLaC", 4) != 0) {
        return false;
    }

    bool is_last_block = false;
    while (!is_last_block && input.good()) {
        std::uint8_t block_header[4] = {};
        if (!read_exact(input, reinterpret_cast<char*>(block_header), sizeof(block_header))) {
            return false;
        }

        is_last_block = (block_header[0] & 0x80U) != 0;
        const std::uint8_t block_type = static_cast<std::uint8_t>(block_header[0] & 0x7FU);
        const std::uint32_t block_length = read_u24_be(block_header + 1);

        if (block_type == 4) {
            std::vector<std::uint8_t> block(block_length);
            if (!read_exact(input, reinterpret_cast<char*>(block.data()), block.size())) {
                return false;
            }
            parse_vorbis_comment_payload(block.data(), block.size(), metadata);
        } else {
            input.seekg(static_cast<std::streamoff>(block_length), std::ios::cur);
            if (!input.good()) {
                return false;
            }
        }
    }

    return has_metadata_parts(metadata);
}

bool parse_ogg_comments(const std::filesystem::path& media_path,
                        MediaRenameMetadataService::MetadataFields& metadata)
{
    std::ifstream input(media_path, std::ios::binary);
    if (!input) {
        return false;
    }

    enum class OggCodec {
        Unknown,
        Vorbis,
        Opus
    };

    OggCodec codec = OggCodec::Unknown;
    std::vector<std::uint8_t> packet;
    std::size_t packet_index = 0;

    while (input.good()) {
        std::uint8_t page_header[27] = {};
        if (!read_exact(input, reinterpret_cast<char*>(page_header), sizeof(page_header))) {
            break;
        }
        if (std::memcmp(page_header, "OggS", 4) != 0 || page_header[4] != 0) {
            return false;
        }

        const std::uint8_t segment_count = page_header[26];
        std::vector<std::uint8_t> segment_table(segment_count);
        if (segment_count > 0 &&
            !read_exact(input, reinterpret_cast<char*>(segment_table.data()), segment_table.size())) {
            return false;
        }

        std::size_t body_size = 0;
        for (const std::uint8_t segment : segment_table) {
            body_size += segment;
        }

        std::vector<std::uint8_t> body(body_size);
        if (body_size > 0 && !read_exact(input, reinterpret_cast<char*>(body.data()), body.size())) {
            return false;
        }

        std::size_t body_offset = 0;
        for (const std::uint8_t segment : segment_table) {
            packet.insert(packet.end(),
                          body.begin() + static_cast<std::ptrdiff_t>(body_offset),
                          body.begin() + static_cast<std::ptrdiff_t>(body_offset + segment));
            body_offset += segment;

            if (segment == 255) {
                continue;
            }

            if (packet_index == 0) {
                if (packet.size() >= 7 && packet[0] == 0x01 &&
                    std::memcmp(packet.data() + 1, "vorbis", 6) == 0) {
                    codec = OggCodec::Vorbis;
                } else if (packet.size() >= 8 && std::memcmp(packet.data(), "OpusHead", 8) == 0) {
                    codec = OggCodec::Opus;
                } else {
                    return false;
                }
            } else if (packet_index == 1) {
                if (codec == OggCodec::Vorbis &&
                    packet.size() >= 7 &&
                    packet[0] == 0x03 &&
                    std::memcmp(packet.data() + 1, "vorbis", 6) == 0) {
                    return parse_vorbis_comment_payload(packet.data() + 7, packet.size() - 7, metadata);
                }
                if (codec == OggCodec::Opus &&
                    packet.size() >= 8 &&
                    std::memcmp(packet.data(), "OpusTags", 8) == 0) {
                    return parse_vorbis_comment_payload(packet.data() + 8, packet.size() - 8, metadata);
                }
                return false;
            }

            packet.clear();
            ++packet_index;
        }
    }

    return has_metadata_parts(metadata);
}

constexpr std::uint32_t make_fourcc(std::uint8_t c0,
                                    std::uint8_t c1,
                                    std::uint8_t c2,
                                    std::uint8_t c3)
{
    return (static_cast<std::uint32_t>(c0) << 24) |
           (static_cast<std::uint32_t>(c1) << 16) |
           (static_cast<std::uint32_t>(c2) << 8) |
           static_cast<std::uint32_t>(c3);
}

constexpr std::uint32_t kAtomMoov = make_fourcc('m', 'o', 'o', 'v');
constexpr std::uint32_t kAtomUdta = make_fourcc('u', 'd', 't', 'a');
constexpr std::uint32_t kAtomMeta = make_fourcc('m', 'e', 't', 'a');
constexpr std::uint32_t kAtomIlst = make_fourcc('i', 'l', 's', 't');
constexpr std::uint32_t kAtomData = make_fourcc('d', 'a', 't', 'a');
constexpr std::uint32_t kAtomTrak = make_fourcc('t', 'r', 'a', 'k');
constexpr std::uint32_t kAtomMdia = make_fourcc('m', 'd', 'i', 'a');
constexpr std::uint32_t kAtomMinf = make_fourcc('m', 'i', 'n', 'f');
constexpr std::uint32_t kAtomStbl = make_fourcc('s', 't', 'b', 'l');
constexpr std::uint32_t kAtomEdts = make_fourcc('e', 'd', 't', 's');
constexpr std::uint32_t kAtomDinf = make_fourcc('d', 'i', 'n', 'f');
constexpr std::uint32_t kAtomMvex = make_fourcc('m', 'v', 'e', 'x');
constexpr std::uint32_t kTagName = make_fourcc(0xA9, 'n', 'a', 'm');
constexpr std::uint32_t kTagArtist = make_fourcc(0xA9, 'A', 'R', 'T');
constexpr std::uint32_t kTagAlbumArtist = make_fourcc('a', 'A', 'R', 'T');
constexpr std::uint32_t kTagAlbum = make_fourcc(0xA9, 'a', 'l', 'b');
constexpr std::uint32_t kTagDate = make_fourcc(0xA9, 'd', 'a', 'y');
constexpr std::uint32_t kTagTitle3gp = make_fourcc('t', 'i', 't', 'l');
constexpr std::uint32_t kTagArtist3gp = make_fourcc('a', 'u', 't', 'h');
constexpr std::uint32_t kTagAlbum3gp = make_fourcc('a', 'l', 'b', 'm');
constexpr std::uint32_t kTagYear3gp = make_fourcc('y', 'r', 'r', 'c');

bool seek_to(std::istream& input, std::uint64_t offset)
{
    constexpr std::uint64_t kMaxStreamOff =
        static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max());
    if (offset > kMaxStreamOff) {
        return false;
    }

    input.clear();
    input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    return input.good();
}

bool read_at(std::istream& input, std::uint64_t offset, std::uint8_t* destination, std::size_t size)
{
    if (destination == nullptr || size == 0) {
        return true;
    }
    if (!seek_to(input, offset)) {
        return false;
    }
    return read_exact(input, reinterpret_cast<char*>(destination), size);
}

bool is_mp4_container_atom(std::uint32_t atom_type)
{
    return atom_type == kAtomMoov ||
           atom_type == kAtomUdta ||
           atom_type == kAtomMeta ||
           atom_type == kAtomIlst ||
           atom_type == kAtomTrak ||
           atom_type == kAtomMdia ||
           atom_type == kAtomMinf ||
           atom_type == kAtomStbl ||
           atom_type == kAtomEdts ||
           atom_type == kAtomDinf ||
           atom_type == kAtomMvex;
}

bool is_mp4_metadata_key_atom(std::uint32_t atom_type)
{
    return atom_type == kTagName ||
           atom_type == kTagArtist ||
           atom_type == kTagAlbumArtist ||
           atom_type == kTagAlbum ||
           atom_type == kTagDate ||
           atom_type == kTagTitle3gp ||
           atom_type == kTagArtist3gp ||
           atom_type == kTagAlbum3gp ||
           atom_type == kTagYear3gp;
}

std::string decode_utf16_ascii_fallback(const std::uint8_t* bytes, std::size_t size)
{
    if (bytes == nullptr || size < 2) {
        return std::string();
    }

    bool little_endian = false;
    std::size_t offset = 0;
    if (size >= 2) {
        if (bytes[0] == 0xFF && bytes[1] == 0xFE) {
            little_endian = true;
            offset = 2;
        } else if (bytes[0] == 0xFE && bytes[1] == 0xFF) {
            little_endian = false;
            offset = 2;
        }
    }

    std::string decoded;
    decoded.reserve(size / 2);
    for (std::size_t index = offset; index + 1 < size; index += 2) {
        const std::uint16_t code_unit = little_endian
                                            ? static_cast<std::uint16_t>(bytes[index]) |
                                                  (static_cast<std::uint16_t>(bytes[index + 1]) << 8)
                                            : (static_cast<std::uint16_t>(bytes[index]) << 8) |
                                                  static_cast<std::uint16_t>(bytes[index + 1]);
        if (code_unit == 0) {
            break;
        }
        if (code_unit <= 0x7F) {
            decoded.push_back(static_cast<char>(code_unit));
        } else {
            decoded.push_back(' ');
        }
    }

    return sanitize_metadata_value(decoded);
}

std::optional<std::string> parse_mp4_data_text(const std::vector<std::uint8_t>& payload)
{
    if (payload.size() < 8) {
        return std::nullopt;
    }

    const std::uint32_t marker_a = read_u32_be(payload.data());
    const std::uint32_t marker_b = read_u32_be(payload.data() + 4);

    std::uint32_t data_type = marker_a;
    if ((marker_a == 0 || marker_a > 32) &&
        (marker_b == 1 || marker_b == 2 || marker_b == 0 || marker_b == 21)) {
        data_type = marker_b;
    }

    const std::uint8_t* text_bytes = payload.data() + 8;
    const std::size_t text_size = payload.size() - 8;
    if (text_size == 0) {
        return std::nullopt;
    }

    std::string decoded;
    if (data_type == 2) {
        decoded = decode_utf16_ascii_fallback(text_bytes, text_size);
    } else {
        decoded.assign(reinterpret_cast<const char*>(text_bytes), text_size);
        if (const auto null_index = decoded.find('\0'); null_index != std::string::npos) {
            decoded.resize(null_index);
        }
        decoded = sanitize_metadata_value(decoded);
    }

    if (decoded.empty()) {
        return std::nullopt;
    }
    return decoded;
}

void assign_mp4_metadata_field(std::uint32_t atom_type,
                               const std::string& value,
                               MediaRenameMetadataService::MetadataFields& metadata)
{
    if (value.empty()) {
        return;
    }

    if (atom_type == kTagName || atom_type == kTagTitle3gp) {
        assign_if_missing(metadata.title, value);
        return;
    }
    if (atom_type == kTagArtist || atom_type == kTagAlbumArtist || atom_type == kTagArtist3gp) {
        assign_if_missing(metadata.artist, value);
        return;
    }
    if (atom_type == kTagAlbum || atom_type == kTagAlbum3gp) {
        assign_if_missing(metadata.album, value);
        return;
    }
    if (atom_type == kTagDate || atom_type == kTagYear3gp) {
        assign_if_missing(metadata.year, value);
    }
}

void parse_mp4_atoms(std::istream& input,
                     std::uint64_t range_start,
                     std::uint64_t range_end,
                     std::optional<std::uint32_t> current_tag,
                     std::size_t depth,
                     MediaRenameMetadataService::MetadataFields& metadata)
{
    if (depth > 12 || range_start >= range_end) {
        return;
    }

    std::uint64_t offset = range_start;
    while (offset + 8 <= range_end) {
        std::uint8_t header[16] = {};
        if (!read_at(input, offset, header, 8)) {
            return;
        }

        std::uint64_t atom_size = read_u32_be(header);
        const std::uint32_t atom_type = read_u32_be(header + 4);
        std::uint64_t atom_header_size = 8;

        if (atom_size == 1) {
            if (!read_at(input, offset + 8, header + 8, 8)) {
                return;
            }
            atom_size = read_u64_be(header + 8);
            atom_header_size = 16;
        } else if (atom_size == 0) {
            atom_size = range_end - offset;
        }

        if (atom_size < atom_header_size) {
            return;
        }
        const std::uint64_t atom_end = offset + atom_size;
        if (atom_end < offset || atom_end > range_end) {
            return;
        }

        std::uint64_t payload_start = offset + atom_header_size;
        const std::uint64_t payload_end = atom_end;

        if (atom_type == kAtomMeta) {
            if (payload_start + 4 > payload_end) {
                offset = atom_end;
                continue;
            }
            payload_start += 4;
        }

        if (atom_type == kAtomData && current_tag.has_value()) {
            const std::uint64_t payload_size = payload_end - payload_start;
            if (payload_size >= 8 && payload_size <= 64 * 1024) {
                std::vector<std::uint8_t> data(static_cast<std::size_t>(payload_size));
                if (read_at(input, payload_start, data.data(), data.size())) {
                    if (const auto text = parse_mp4_data_text(data)) {
                        assign_mp4_metadata_field(*current_tag, *text, metadata);
                    }
                }
            }
        }

        bool descend = false;
        std::optional<std::uint32_t> next_tag = current_tag;
        if (is_mp4_metadata_key_atom(atom_type)) {
            descend = true;
            next_tag = atom_type;
        } else if (is_mp4_container_atom(atom_type)) {
            descend = true;
            if (atom_type == kAtomIlst) {
                next_tag.reset();
            }
        } else if (current_tag.has_value() && atom_type != kAtomData) {
            descend = true;
        }

        if (descend && payload_start < payload_end) {
            parse_mp4_atoms(input, payload_start, payload_end, next_tag, depth + 1, metadata);
        }

        if (atom_size == 0) {
            return;
        }
        offset = atom_end;
    }
}

bool parse_mp4_style_metadata(const std::filesystem::path& media_path,
                              MediaRenameMetadataService::MetadataFields& metadata)
{
    std::ifstream input(media_path, std::ios::binary);
    if (!input) {
        return false;
    }

    input.seekg(0, std::ios::end);
    const std::streamoff file_size = input.tellg();
    if (file_size <= 0) {
        return false;
    }

    parse_mp4_atoms(input,
                    0,
                    static_cast<std::uint64_t>(file_size),
                    std::nullopt,
                    0,
                    metadata);
    return has_metadata_parts(metadata);
}

std::optional<MediaRenameMetadataService::MetadataFields> parse_media_metadata_without_mediainfo(
    const std::filesystem::path& media_path)
{
    if (!MediaRenameMetadataService::is_supported_media(media_path)) {
        return std::nullopt;
    }

    MediaRenameMetadataService::MetadataFields metadata;
    const std::string extension = to_lower_copy(Utils::path_to_utf8(media_path.extension()));

    if (is_supported_audio(media_path)) {
        parse_id3v2_from_file(media_path, metadata);

        if (extension == ".mp3") {
            parse_id3v1_from_file(media_path, metadata);
        } else if (extension == ".flac") {
            parse_flac_vorbis_comments(media_path, metadata);
        } else if (extension == ".ogg" || extension == ".oga" || extension == ".opus") {
            parse_ogg_comments(media_path, metadata);
        } else if (extension == ".m4a") {
            parse_mp4_style_metadata(media_path, metadata);
        }
    }

    if (is_supported_video(media_path)) {
        if (extension == ".mp4" || extension == ".m4v" || extension == ".mov" || extension == ".3gp") {
            parse_mp4_style_metadata(media_path, metadata);
        }
    }

    if (!has_metadata_parts(metadata)) {
        return std::nullopt;
    }
    return metadata;
}
#endif

#ifdef AI_FILE_SORTER_USE_MEDIAINFOLIB
std::string media_info_to_utf8(const MediaInfoCompat::String& value)
{
#if defined(UNICODE) || defined(_UNICODE)
    const std::wstring wide_value(value.begin(), value.end());
    return trim_copy(QString::fromStdWString(wide_value).toUtf8().toStdString());
#else
    return trim_copy(std::string(value.begin(), value.end()));
#endif
}

std::string query_field(MediaInfoCompat::MediaInfo& media_info,
                        MediaInfoCompat::stream_t stream_kind,
                        size_t stream_number,
                        const MediaInfoCompat::Char* parameter)
{
    return media_info_to_utf8(media_info.Get(stream_kind,
                                             stream_number,
                                             parameter,
                                             MediaInfoCompat::Info_Text,
                                             MediaInfoCompat::Info_Name));
}

template <size_t N>
std::optional<std::string> query_first_field(MediaInfoCompat::MediaInfo& media_info,
                                             MediaInfoCompat::stream_t stream_kind,
                                             size_t stream_number,
                                             const std::array<const MediaInfoCompat::Char*, N>& keys)
{
    for (const auto* key : keys) {
        const std::string value = query_field(media_info, stream_kind, stream_number, key);
        if (!value.empty()) {
            return value;
        }
    }
    return std::nullopt;
}

std::optional<std::string> query_first_available(MediaInfoCompat::MediaInfo& media_info,
                                                 const std::vector<std::pair<MediaInfoCompat::stream_t,
                                                                             std::array<const MediaInfoCompat::Char*, 6>>>& probes)
{
    for (const auto& probe : probes) {
        if (auto value = query_first_field(media_info, probe.first, 0, probe.second)) {
            return value;
        }
    }
    return std::nullopt;
}
#endif

} // namespace

std::optional<std::string> MediaRenameMetadataService::suggest_name(const std::filesystem::path& media_path) const
{
    if (!is_supported_media(media_path)) {
        return std::nullopt;
    }

    const auto metadata = extract_metadata(media_path);
    if (!metadata.has_value()) {
        return std::nullopt;
    }

    const std::string suggested = compose_filename(media_path, *metadata);
    if (suggested.empty()) {
        return std::nullopt;
    }

    const std::string original = Utils::path_to_utf8(media_path.filename());
    if (to_lower_copy(suggested) == to_lower_copy(original)) {
        return std::nullopt;
    }

    return suggested;
}

bool MediaRenameMetadataService::is_supported_media(const std::filesystem::path& path)
{
    return is_supported_audio(path) || is_supported_video(path);
}

std::string MediaRenameMetadataService::compose_filename(const std::filesystem::path& original_path,
                                                         const MetadataFields& metadata)
{
    const std::string original_name = Utils::path_to_utf8(original_path.filename());
    if (original_name.empty()) {
        return std::string();
    }
    if (!has_metadata_parts(metadata)) {
        return original_name;
    }

    const std::string extension = Utils::path_to_utf8(original_path.extension());
    const std::string fallback_stem = Utils::path_to_utf8(original_path.stem());

    std::vector<std::string> parts;
    parts.reserve(4);

    if (metadata.year.has_value()) {
        if (const auto year = normalize_year(*metadata.year)) {
            parts.push_back(*year);
        }
    }

    const auto append_slug = [&parts](const std::optional<std::string>& value) {
        if (!value.has_value() || value->empty()) {
            return;
        }
        const std::string slug = MediaRenameMetadataService::slugify(*value);
        if (!slug.empty()) {
            parts.push_back(slug);
        }
    };

    append_slug(metadata.artist);
    append_slug(metadata.album);

    std::string title_slug;
    if (metadata.title.has_value()) {
        title_slug = slugify(*metadata.title);
    }
    if (title_slug.empty()) {
        title_slug = slugify(fallback_stem);
    }
    if (!title_slug.empty()) {
        parts.push_back(title_slug);
    }

    if (parts.empty()) {
        return original_name;
    }

    std::string base_name;
    for (size_t index = 0; index < parts.size(); ++index) {
        if (index > 0) {
            base_name.push_back('_');
        }
        base_name += parts[index];
    }

    if (base_name.empty()) {
        return original_name;
    }

    return extension.empty() ? base_name : base_name + extension;
}

std::optional<MediaRenameMetadataService::MetadataFields> MediaRenameMetadataService::extract_metadata(
    const std::filesystem::path& media_path)
{
#ifndef AI_FILE_SORTER_USE_MEDIAINFOLIB
    auto metadata = parse_media_metadata_without_mediainfo(media_path);
    if (!metadata.has_value()) {
        return std::nullopt;
    }

    if (metadata->year.has_value()) {
        metadata->year = normalize_year(*metadata->year);
        if (!metadata->year.has_value()) {
            metadata->year.reset();
        }
    }

    if (!has_metadata_parts(*metadata)) {
        return std::nullopt;
    }

    return metadata;
#else
    if (!is_supported_media(media_path)) {
        return std::nullopt;
    }

    MediaInfoCompat::MediaInfo media_info;

#if defined(UNICODE) || defined(_UNICODE)
    const std::wstring wide_path = media_path.wstring();
    const MediaInfoCompat::String open_path(wide_path.begin(), wide_path.end());
#else
    const std::string utf8_path = Utils::path_to_utf8(media_path);
    const MediaInfoCompat::String open_path(utf8_path.begin(), utf8_path.end());
#endif

    if (media_info.Open(open_path) == 0) {
        return std::nullopt;
    }

    MetadataFields metadata;

    const bool audio_file = is_supported_audio(media_path);
    const bool video_file = is_supported_video(media_path);

    if (audio_file) {
        static constexpr std::array<const MediaInfoCompat::Char*, 6> kArtistGeneral = {
            __T("Performer"),
            __T("Album_Artist"),
            __T("Artist"),
            __T("Composer"),
            __T("Track/Performer"),
            __T("Track/Artist")
        };
        static constexpr std::array<const MediaInfoCompat::Char*, 6> kArtistAudio = {
            __T("Performer"),
            __T("Album_Artist"),
            __T("Artist"),
            __T("Composer"),
            __T("Track/Performer"),
            __T("Track/Artist")
        };
        static constexpr std::array<const MediaInfoCompat::Char*, 6> kAlbumGeneral = {
            __T("Album"),
            __T("Track/Album"),
            __T("Movie"),
            __T("Collection"),
            __T("Show"),
            __T("PackageName")
        };
        static constexpr std::array<const MediaInfoCompat::Char*, 6> kTitleGeneral = {
            __T("Title"),
            __T("Track"),
            __T("Track/Title"),
            __T("Song"),
            __T("Movie"),
            __T("Name")
        };
        static constexpr std::array<const MediaInfoCompat::Char*, 6> kTitleAudio = {
            __T("Title"),
            __T("Track"),
            __T("Track/Title"),
            __T("Song"),
            __T("Name"),
            __T("Encoded_Library/Name")
        };

        metadata.artist = query_first_field(media_info, MediaInfoCompat::Stream_General, 0, kArtistGeneral);
        if (!metadata.artist.has_value()) {
            metadata.artist = query_first_field(media_info, MediaInfoCompat::Stream_Audio, 0, kArtistAudio);
        }

        metadata.album = query_first_field(media_info, MediaInfoCompat::Stream_General, 0, kAlbumGeneral);

        metadata.title = query_first_field(media_info, MediaInfoCompat::Stream_General, 0, kTitleGeneral);
        if (!metadata.title.has_value()) {
            metadata.title = query_first_field(media_info, MediaInfoCompat::Stream_Audio, 0, kTitleAudio);
        }
    }

    if (video_file) {
        static constexpr std::array<const MediaInfoCompat::Char*, 6> kArtistVideoGeneral = {
            __T("Performer"),
            __T("Director"),
            __T("Composer"),
            __T("Producer"),
            __T("Artist"),
            __T("Encoded_Library/Name")
        };
        static constexpr std::array<const MediaInfoCompat::Char*, 6> kAlbumVideoGeneral = {
            __T("Movie"),
            __T("Show"),
            __T("Album"),
            __T("Collection"),
            __T("Season"),
            __T("PackageName")
        };
        static constexpr std::array<const MediaInfoCompat::Char*, 6> kTitleVideoGeneral = {
            __T("Title"),
            __T("Movie"),
            __T("Track"),
            __T("Episode"),
            __T("Name"),
            __T("Series")
        };
        static constexpr std::array<const MediaInfoCompat::Char*, 6> kTitleVideoStream = {
            __T("Title"),
            __T("Track"),
            __T("Name"),
            __T("Movie"),
            __T("Episode"),
            __T("Series")
        };

        if (!metadata.artist.has_value()) {
            metadata.artist = query_first_field(media_info,
                                                MediaInfoCompat::Stream_General,
                                                0,
                                                kArtistVideoGeneral);
        }
        if (!metadata.album.has_value()) {
            metadata.album = query_first_field(media_info,
                                               MediaInfoCompat::Stream_General,
                                               0,
                                               kAlbumVideoGeneral);
        }
        if (!metadata.title.has_value()) {
            metadata.title = query_first_field(media_info,
                                               MediaInfoCompat::Stream_General,
                                               0,
                                               kTitleVideoGeneral);
        }
        if (!metadata.title.has_value()) {
            metadata.title = query_first_field(media_info,
                                               MediaInfoCompat::Stream_Video,
                                               0,
                                               kTitleVideoStream);
        }
    }

    static const std::vector<std::pair<MediaInfoCompat::stream_t,
                                       std::array<const MediaInfoCompat::Char*, 6>>> kYearProbes = {
        {
            MediaInfoCompat::Stream_General,
            {
                __T("Recorded_Date"),
                __T("Released_Date"),
                __T("Encoded_Date"),
                __T("Mastered_Date"),
                __T("Tagged_Date"),
                __T("Date")
            }
        },
        {
            MediaInfoCompat::Stream_Audio,
            {
                __T("Recorded_Date"),
                __T("Released_Date"),
                __T("Encoded_Date"),
                __T("Date"),
                __T("Mastered_Date"),
                __T("Tagged_Date")
            }
        },
        {
            MediaInfoCompat::Stream_Video,
            {
                __T("Recorded_Date"),
                __T("Released_Date"),
                __T("Encoded_Date"),
                __T("Date"),
                __T("Mastered_Date"),
                __T("Tagged_Date")
            }
        }
    };

    if (const auto raw_year = query_first_available(media_info, kYearProbes)) {
        metadata.year = normalize_year(*raw_year);
    }

    media_info.Close();

    if (!has_metadata_parts(metadata)) {
        return std::nullopt;
    }

    return metadata;
#endif
}

std::string MediaRenameMetadataService::slugify(const std::string& value)
{
    std::string slug;
    slug.reserve(value.size());
    bool last_separator = false;

    for (unsigned char ch : value) {
        if (std::isalnum(ch)) {
            slug.push_back(static_cast<char>(std::tolower(ch)));
            last_separator = false;
        } else if (!last_separator && !slug.empty()) {
            slug.push_back('_');
            last_separator = true;
        }
    }

    while (!slug.empty() && slug.back() == '_') {
        slug.pop_back();
    }

    return slug;
}

std::optional<std::string> MediaRenameMetadataService::normalize_year(const std::string& value)
{
    if (value.size() < 4) {
        return std::nullopt;
    }

    for (size_t index = 0; index + 3 < value.size(); ++index) {
        const char c0 = value[index];
        const char c1 = value[index + 1];
        const char c2 = value[index + 2];
        const char c3 = value[index + 3];

        if (!std::isdigit(static_cast<unsigned char>(c0)) ||
            !std::isdigit(static_cast<unsigned char>(c1)) ||
            !std::isdigit(static_cast<unsigned char>(c2)) ||
            !std::isdigit(static_cast<unsigned char>(c3))) {
            continue;
        }

        const bool prefixed_by_digit =
            index > 0 && std::isdigit(static_cast<unsigned char>(value[index - 1]));
        const bool suffixed_by_digit =
            (index + 4) < value.size() &&
            std::isdigit(static_cast<unsigned char>(value[index + 4]));
        if (prefixed_by_digit || suffixed_by_digit) {
            continue;
        }

        const int year = (c0 - '0') * 1000 +
                         (c1 - '0') * 100 +
                         (c2 - '0') * 10 +
                         (c3 - '0');
        if (year >= 1900 && year <= 2100) {
            return std::string{c0, c1, c2, c3};
        }
    }

    return std::nullopt;
}
