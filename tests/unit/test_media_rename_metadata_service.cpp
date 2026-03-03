#include <catch2/catch_test_macros.hpp>

#include "MediaRenameMetadataService.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

TEST_CASE("MediaRenameMetadataService composes year-artist-album-title filenames") {
    MediaRenameMetadataService::MetadataFields metadata;
    metadata.year = "2014";
    metadata.artist = "Massive Attack";
    metadata.album = "Mezzanine";
    metadata.title = "Teardrop";

    const std::filesystem::path input{"/tmp/track01.mp3"};
    const std::string output = MediaRenameMetadataService::compose_filename(input, metadata);

    REQUIRE(output == "2014_massive_attack_mezzanine_teardrop.mp3");
}

TEST_CASE("MediaRenameMetadataService falls back to source stem when title is missing") {
    MediaRenameMetadataService::MetadataFields metadata;
    metadata.year = "1997";
    metadata.artist = "Daft Punk";

    const std::filesystem::path input{"/tmp/Around The World.flac"};
    const std::string output = MediaRenameMetadataService::compose_filename(input, metadata);

    REQUIRE(output == "1997_daft_punk_around_the_world.flac");
}

TEST_CASE("MediaRenameMetadataService keeps original filename when metadata is absent") {
    const MediaRenameMetadataService::MetadataFields metadata{};
    const std::filesystem::path input{"/tmp/video_clip.mp4"};

    const std::string output = MediaRenameMetadataService::compose_filename(input, metadata);

    REQUIRE(output == "video_clip.mp4");
}

#ifndef AI_FILE_SORTER_USE_MEDIAINFOLIB
namespace {

class TempMediaFile {
public:
    explicit TempMediaFile(const std::string& file_name)
    {
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("aifs_media_meta_" + std::to_string(now) + "_" + file_name);
    }

    ~TempMediaFile()
    {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }

    const std::filesystem::path& path() const
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};

void write_binary_file(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    REQUIRE(output.is_open());
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    REQUIRE(output.good());
}

void append_le_u32(std::vector<std::uint8_t>& output, std::uint32_t value)
{
    output.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    output.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFU));
    output.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFU));
    output.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFU));
}

void append_be_u32(std::vector<std::uint8_t>& output, std::uint32_t value)
{
    output.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFU));
    output.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFU));
    output.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFU));
    output.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

void append_le_u64(std::vector<std::uint8_t>& output, std::uint64_t value)
{
    for (int index = 0; index < 8; ++index) {
        output.push_back(static_cast<std::uint8_t>((value >> (index * 8)) & 0xFFU));
    }
}

std::vector<std::uint8_t> make_mp4_atom(const std::array<std::uint8_t, 4>& type,
                                        const std::vector<std::uint8_t>& payload)
{
    std::vector<std::uint8_t> atom;
    append_be_u32(atom, static_cast<std::uint32_t>(8 + payload.size()));
    atom.insert(atom.end(), type.begin(), type.end());
    atom.insert(atom.end(), payload.begin(), payload.end());
    return atom;
}

std::vector<std::uint8_t> make_mp4_data_atom(const std::string& value)
{
    std::vector<std::uint8_t> payload;
    append_be_u32(payload, 1); // UTF-8 data type
    append_be_u32(payload, 0); // locale
    payload.insert(payload.end(), value.begin(), value.end());
    return make_mp4_atom({'d', 'a', 't', 'a'}, payload);
}

std::vector<std::uint8_t> make_mp4_metadata_atom(const std::array<std::uint8_t, 4>& key,
                                                 const std::string& value)
{
    return make_mp4_atom(key, make_mp4_data_atom(value));
}

void write_fixed_id3v1_field(std::array<char, 128>& tag,
                             std::size_t offset,
                             std::size_t length,
                             const std::string& value)
{
    const std::size_t copy_length = std::min(length, value.size());
    std::copy_n(value.begin(), copy_length, tag.begin() + static_cast<std::ptrdiff_t>(offset));
}

std::vector<std::uint8_t> make_mp3_with_id3v1()
{
    std::vector<std::uint8_t> bytes(64, 0);
    std::array<char, 128> tag{};
    tag[0] = 'T';
    tag[1] = 'A';
    tag[2] = 'G';
    write_fixed_id3v1_field(tag, 3, 30, "Celestial Echoes");
    write_fixed_id3v1_field(tag, 33, 30, "Synth Unit");
    write_fixed_id3v1_field(tag, 63, 30, "Moon Tides");
    write_fixed_id3v1_field(tag, 93, 4, "2024");
    bytes.insert(bytes.end(), tag.begin(), tag.end());
    return bytes;
}

std::vector<std::uint8_t> make_vorbis_comment_payload(const std::vector<std::string>& comments)
{
    std::vector<std::uint8_t> payload;
    const std::string vendor = "AIFileSorterTests";
    append_le_u32(payload, static_cast<std::uint32_t>(vendor.size()));
    payload.insert(payload.end(), vendor.begin(), vendor.end());
    append_le_u32(payload, static_cast<std::uint32_t>(comments.size()));
    for (const auto& comment : comments) {
        append_le_u32(payload, static_cast<std::uint32_t>(comment.size()));
        payload.insert(payload.end(), comment.begin(), comment.end());
    }
    return payload;
}

std::vector<std::uint8_t> make_flac_with_vorbis_comments()
{
    const std::vector<std::uint8_t> comments = make_vorbis_comment_payload({
        "TITLE=Celestial Echoes Continued",
        "ARTIST=Synth Unit",
        "ALBUM=Moon Tides",
        "DATE=2023-04-09"
    });

    std::vector<std::uint8_t> bytes;
    bytes.insert(bytes.end(), {'f', 'L', 'a', 'C'});
    bytes.push_back(0x84); // last metadata block + vorbis comment block type
    bytes.push_back(static_cast<std::uint8_t>((comments.size() >> 16) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((comments.size() >> 8) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>(comments.size() & 0xFFU));
    bytes.insert(bytes.end(), comments.begin(), comments.end());
    return bytes;
}

std::vector<std::uint8_t> make_opus_with_tags()
{
    std::vector<std::uint8_t> opus_head = {
        'O', 'p', 'u', 's', 'H', 'e', 'a', 'd',
        1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    std::vector<std::uint8_t> opus_tags = {'O', 'p', 'u', 's', 'T', 'a', 'g', 's'};
    const std::vector<std::uint8_t> comments = make_vorbis_comment_payload({
        "TITLE=Celestial Echoes 2",
        "ARTIST=Synth Unit",
        "ALBUM=Celestial Sets",
        "YEAR=2022"
    });
    opus_tags.insert(opus_tags.end(), comments.begin(), comments.end());

    REQUIRE(opus_head.size() < 255);
    REQUIRE(opus_tags.size() < 255);

    std::vector<std::uint8_t> bytes;
    bytes.insert(bytes.end(), {'O', 'g', 'g', 'S'});
    bytes.push_back(0);     // version
    bytes.push_back(0x02);  // BOS page
    append_le_u64(bytes, 0);
    append_le_u32(bytes, 1); // serial number
    append_le_u32(bytes, 0); // sequence number
    append_le_u32(bytes, 0); // checksum (not validated by parser)
    bytes.push_back(2);      // segment count
    bytes.push_back(static_cast<std::uint8_t>(opus_head.size()));
    bytes.push_back(static_cast<std::uint8_t>(opus_tags.size()));
    bytes.insert(bytes.end(), opus_head.begin(), opus_head.end());
    bytes.insert(bytes.end(), opus_tags.begin(), opus_tags.end());
    return bytes;
}

std::vector<std::uint8_t> make_mp4_with_ilst_tags()
{
    std::vector<std::uint8_t> ilst_payload;
    const std::uint8_t copyright = 0xA9;
    const std::array<std::uint8_t, 4> title_tag = {copyright, 'n', 'a', 'm'};
    const std::array<std::uint8_t, 4> artist_tag = {copyright, 'A', 'R', 'T'};
    const std::array<std::uint8_t, 4> album_tag = {copyright, 'a', 'l', 'b'};
    const std::array<std::uint8_t, 4> date_tag = {copyright, 'd', 'a', 'y'};

    const auto title_atom = make_mp4_metadata_atom(title_tag, "Celestial Echoes Video");
    ilst_payload.insert(ilst_payload.end(), title_atom.begin(), title_atom.end());
    const auto artist_atom = make_mp4_metadata_atom(artist_tag, "Synth Unit");
    ilst_payload.insert(ilst_payload.end(), artist_atom.begin(), artist_atom.end());
    const auto album_atom = make_mp4_metadata_atom(album_tag, "Moon Tides Live");
    ilst_payload.insert(ilst_payload.end(), album_atom.begin(), album_atom.end());
    const auto date_atom = make_mp4_metadata_atom(date_tag, "2021-06-30");
    ilst_payload.insert(ilst_payload.end(), date_atom.begin(), date_atom.end());

    const auto ilst_atom = make_mp4_atom({'i', 'l', 's', 't'}, ilst_payload);

    std::vector<std::uint8_t> meta_payload(4, 0); // version + flags
    meta_payload.insert(meta_payload.end(), ilst_atom.begin(), ilst_atom.end());
    const auto meta_atom = make_mp4_atom({'m', 'e', 't', 'a'}, meta_payload);
    const auto udta_atom = make_mp4_atom({'u', 'd', 't', 'a'}, meta_atom);
    const auto moov_atom = make_mp4_atom({'m', 'o', 'o', 'v'}, udta_atom);

    std::vector<std::uint8_t> ftyp_payload = {
        'i', 's', 'o', 'm', 0, 0, 0, 1, 'i', 's', 'o', 'm', 'm', 'p', '4', '1'
    };
    const auto ftyp_atom = make_mp4_atom({'f', 't', 'y', 'p'}, ftyp_payload);

    std::vector<std::uint8_t> file;
    file.insert(file.end(), ftyp_atom.begin(), ftyp_atom.end());
    file.insert(file.end(), moov_atom.begin(), moov_atom.end());
    return file;
}

} // namespace

TEST_CASE("MediaRenameMetadataService extracts MP3 ID3v1 metadata when MediaInfo is unavailable")
{
    TempMediaFile file("raw_track.mp3");
    write_binary_file(file.path(), make_mp3_with_id3v1());

    MediaRenameMetadataService service;
    const auto suggestion = service.suggest_name(file.path());

    REQUIRE(suggestion.has_value());
    REQUIRE(*suggestion == "2024_synth_unit_moon_tides_celestial_echoes.mp3");
}

TEST_CASE("MediaRenameMetadataService extracts FLAC Vorbis comments when MediaInfo is unavailable")
{
    TempMediaFile file("raw_track.flac");
    write_binary_file(file.path(), make_flac_with_vorbis_comments());

    MediaRenameMetadataService service;
    const auto suggestion = service.suggest_name(file.path());

    REQUIRE(suggestion.has_value());
    REQUIRE(*suggestion == "2023_synth_unit_moon_tides_celestial_echoes_continued.flac");
}

TEST_CASE("MediaRenameMetadataService extracts OpusTags when MediaInfo is unavailable")
{
    TempMediaFile file("raw_track.opus");
    write_binary_file(file.path(), make_opus_with_tags());

    MediaRenameMetadataService service;
    const auto suggestion = service.suggest_name(file.path());

    REQUIRE(suggestion.has_value());
    REQUIRE(*suggestion == "2022_synth_unit_celestial_sets_celestial_echoes_2.opus");
}

TEST_CASE("MediaRenameMetadataService extracts MP4 tags when MediaInfo is unavailable")
{
    TempMediaFile file("video_clip.mp4");
    write_binary_file(file.path(), make_mp4_with_ilst_tags());

    MediaRenameMetadataService service;
    const auto suggestion = service.suggest_name(file.path());

    REQUIRE(suggestion.has_value());
    REQUIRE(*suggestion == "2021_synth_unit_moon_tides_live_celestial_echoes_video.mp4");
}
#endif
