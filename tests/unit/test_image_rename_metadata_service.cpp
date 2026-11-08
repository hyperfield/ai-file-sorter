#include <catch2/catch_test_macros.hpp>

#include "ImageRenameMetadataService.hpp"
#include "ImageMetadataTestFixtures.hpp"
#include "ChatCompletionTestServer.hpp"
#include "TestHelpers.hpp"

#include <catch2/generators/catch_generators.hpp>
#include <QUrl>
#include <QUrlQuery>
#include <sqlite3.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <locale>
#include <tuple>
#include <optional>
#include <string>
#include <vector>

namespace {
struct CommaDecimal : std::numpunct<char> {
    char do_decimal_point() const override { return ','; }
};
struct NumericLocaleGuard {
    std::locale previous{std::locale()};
    NumericLocaleGuard() { std::locale::global(std::locale(previous, new CommaDecimal)); }
    ~NumericLocaleGuard() { std::locale::global(previous); }
};
} // namespace

TEST_CASE("Image metadata requires an explicit opt-in before any place lookup", "[geolocation]")
{
    QtAppContext qt;
    TempDir config, photos;
    ChatCompletionTestServer server;
    EnvVarGuard endpoint("AI_FILE_SORTER_NOMINATIM_URL", server.url());
    const auto image = photos.path() / "gps.tiff";
    ImageMetadataTestFixtures::write_gps_tiff(image, 10.1234567, 20.7654321);
    int network_checks = 0;
    ImageRenameMetadataService service(config.path().string(), {
        .allow_online_place_lookup = false,
        .network_available = [&] { ++network_checks; return true; }});
    const auto name = server.run([&] { return service.enrich_suggested_name(image, "photo.tiff"); });
    CHECK(name == "2020-01-02_photo.tiff");
    CHECK(network_checks == 0);
    CHECK(server.requests.empty());
    ImageRenameMetadataService defaults(config.path().string());
    CHECK(defaults.enrich_suggested_name(image, "photo.tiff") == "2020-01-02_photo.tiff");
}

TEST_CASE("Image capture-date extraction never performs a place lookup", "[geolocation]")
{
    TempDir config, photos;
    const auto image = photos.path() / "gps.tiff";
    ImageMetadataTestFixtures::write_gps_tiff(image, 10.1234567, 20.7654321);
    int network_checks = 0;
    ImageRenameMetadataService service(config.path().string(), {
        .allow_online_place_lookup = true,
        .network_available = [&] { ++network_checks; return true; }});
    CHECK(service.extract_capture_date(image) == "2020-01-02");
    CHECK(network_checks == 0);
}

TEST_CASE("Geocoding transmits the same rounded coordinates used by its cache", "[geolocation]")
{
    const auto [latitude, longitude, lat_key, lon_key] = GENERATE(
        std::tuple{10.1234567, 20.7654321, "10.123", "20.765"},
        std::tuple{-10.1234567, -20.7654321, "-10.123", "-20.765"},
        std::tuple{10.1236, 20.7656, "10.124", "20.766"},
        std::tuple{-0.0004, -0.0003, "0.000", "0.000"});
    QtAppContext qt;
    TempDir config, photos;
    ChatCompletionTestServer server;
    EnvVarGuard endpoint("AI_FILE_SORTER_NOMINATIM_URL", server.url());
    server.respond = [](const auto&) -> ChatCompletionTestServer::Reply {
        return {200, R"({"address":{"city":"Rounded Place"}})"};
    };
    const auto image = photos.path() / "gps.tiff";
    ImageMetadataTestFixtures::write_gps_tiff(image, latitude, longitude);
    ImageRenameMetadataService service(config.path().string(), {true, [] { return true; }});
    NumericLocaleGuard locale;
    CHECK(server.run([&] { return service.enrich_suggested_name(image, "photo.tiff"); })
          == "2020-01-02_rounded_place_photo.tiff");
    REQUIRE(server.requests.size() == 1);
    const auto target = server.requests.front().headers.split('\n').front().split(' ').at(1);
    const QUrlQuery query(QUrl(QStringLiteral("http://localhost") + QString::fromUtf8(target)));
    CHECK(query.queryItemValue("lat").toStdString() == lat_key);
    CHECK(query.queryItemValue("lon").toStdString() == lon_key);
    CHECK(query.queryItemValue("zoom") == "10");
    sqlite3* raw_db = nullptr;
    REQUIRE(sqlite3_open((config.path() / "image_place_cache.db").string().c_str(), &raw_db) == SQLITE_OK);
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw_db, sqlite3_close);
    sqlite3_stmt* raw_stmt = nullptr;
    REQUIRE(sqlite3_prepare_v2(db.get(), "SELECT latitude_key, longitude_key FROM reverse_geocode_cache;",
                             -1, &raw_stmt, nullptr) == SQLITE_OK);
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> stmt(raw_stmt, sqlite3_finalize);
    REQUIRE(sqlite3_step(stmt.get()) == SQLITE_ROW);
    CHECK(std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0))) == lat_key);
    CHECK(std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1))) == lon_key);
}

TEST_CASE("Rounded geocoding cache avoids repeat disclosure and respects disabled lookup", "[geolocation]")
{
    QtAppContext qt;
    TempDir config, photos;
    ChatCompletionTestServer server;
    EnvVarGuard endpoint("AI_FILE_SORTER_NOMINATIM_URL", server.url());
    server.respond = [](const auto&) -> ChatCompletionTestServer::Reply {
        return {200, R"({"address":{"city":"Rounded Place"}})"};
    };
    const auto first = photos.path() / "first.tiff", second = photos.path() / "second.tiff";
    ImageMetadataTestFixtures::write_gps_tiff(first, 10.1231, 20.7651);
    ImageMetadataTestFixtures::write_gps_tiff(second, 10.1234, 20.7654);
    ImageRenameMetadataService service(config.path().string(), {true, [] { return true; }});
    const auto names = server.run([&] {
        return std::pair{service.enrich_suggested_name(first, "photo.tiff"),
                         service.enrich_suggested_name(second, "photo.tiff")};
    });
    CHECK(names.first == "2020-01-02_rounded_place_photo.tiff");
    CHECK(names.second == names.first);
    REQUIRE(server.requests.size() == 1);
    ImageRenameMetadataService offline(config.path().string());
    CHECK(offline.enrich_suggested_name(second, "photo.tiff") == "2020-01-02_photo.tiff");
    CHECK(server.requests.size() == 1);
}

namespace {

void append_u16_le(std::vector<uint8_t>& buffer, uint16_t value)
{
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void append_u32_le(std::vector<uint8_t>& buffer, uint32_t value)
{
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void append_u32_be(std::vector<uint8_t>& buffer, uint32_t value)
{
    buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
}

std::vector<uint8_t> make_tiff_with_datetime(const std::string& datetime)
{
    std::string text = datetime;
    text.push_back('\0');

    std::vector<uint8_t> tiff;
    tiff.reserve(8 + 2 + 12 + 4 + text.size());

    // TIFF header (little-endian) + first IFD offset.
    tiff.push_back('I');
    tiff.push_back('I');
    append_u16_le(tiff, 42);
    append_u32_le(tiff, 8);

    // IFD0 with one DateTime entry.
    append_u16_le(tiff, 1);
    append_u16_le(tiff, 0x0132); // DateTime
    append_u16_le(tiff, 2);      // ASCII
    append_u32_le(tiff, static_cast<uint32_t>(text.size()));
    append_u32_le(tiff, 8 + 2 + 12 + 4); // data offset after IFD0 + next-ifd ptr
    append_u32_le(tiff, 0);              // next IFD

    tiff.insert(tiff.end(), text.begin(), text.end());
    return tiff;
}

std::vector<uint8_t> make_png_with_exif_chunk(const std::vector<uint8_t>& tiff_payload)
{
    std::vector<uint8_t> png = {
        0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A
    };

    append_u32_be(png, static_cast<uint32_t>(tiff_payload.size()));
    png.push_back('e');
    png.push_back('X');
    png.push_back('I');
    png.push_back('f');
    png.insert(png.end(), tiff_payload.begin(), tiff_payload.end());
    append_u32_be(png, 0); // CRC is not validated by parser.

    append_u32_be(png, 0);
    png.push_back('I');
    png.push_back('E');
    png.push_back('N');
    png.push_back('D');
    append_u32_be(png, 0);

    return png;
}

void write_binary_file(const std::filesystem::path& path, const std::vector<uint8_t>& bytes)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    REQUIRE(out.good());
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    REQUIRE(out.good());
}

std::filesystem::path make_temp_dir(const std::string& suffix)
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto dir = std::filesystem::temp_directory_path() /
                     ("aifs_image_metadata_" + suffix + "_" + std::to_string(stamp));
    std::filesystem::create_directories(dir);
    return dir;
}

void set_path_value(const std::string& value)
{
#if defined(_WIN32)
    _putenv_s("PATH", value.c_str());
#else
    setenv("PATH", value.c_str(), 1);
#endif
}

void clear_path_value()
{
#if defined(_WIN32)
    _putenv_s("PATH", "");
#else
    unsetenv("PATH");
#endif
}

class PathGuard {
public:
    PathGuard()
    {
        const char* existing = std::getenv("PATH");
        if (existing) {
            had_value_ = true;
            old_value_ = existing;
        }
    }

    ~PathGuard()
    {
        if (had_value_) {
            set_path_value(old_value_);
        } else {
            clear_path_value();
        }
    }

private:
    bool had_value_{false};
    std::string old_value_;
};

} // namespace

TEST_CASE("ImageRenameMetadataService composes date and place prefixes") {
    const std::string actual = ImageRenameMetadataService::apply_prefix_to_filename(
        "black_ducks_row.jpg",
        std::optional<std::string>("2014-03-10"),
        std::optional<std::string>("Venice"));

    CHECK(actual == "2014-03-10_venice_black_ducks_row.jpg");
}

TEST_CASE("ImageRenameMetadataService skips prefix duplication") {
    const std::string already_prefixed = "2014-03-10_venice_black_ducks_row.jpg";
    const std::string actual = ImageRenameMetadataService::apply_prefix_to_filename(
        already_prefixed,
        std::optional<std::string>("2014-03-10"),
        std::optional<std::string>("venice"));

    CHECK(actual == already_prefixed);
}

TEST_CASE("ImageRenameMetadataService sanitizes place labels") {
    const std::string actual = ImageRenameMetadataService::apply_prefix_to_filename(
        "street_market.jpg",
        std::optional<std::string>("2020-01-05"),
        std::optional<std::string>("New York City"));

    CHECK(actual == "2020-01-05_new_york_city_street_market.jpg");
}

TEST_CASE("ImageRenameMetadataService returns original when no prefix data") {
    const std::string original = "sunset.jpg";
    const std::string actual = ImageRenameMetadataService::apply_prefix_to_filename(
        original,
        std::nullopt,
        std::nullopt);

    CHECK(actual == original);
}

TEST_CASE("ImageRenameMetadataService reads TIFF date metadata for rename prefix")
{
    const auto temp_dir = make_temp_dir("tiff");
    const auto image_path = temp_dir / "sample.tiff";
    write_binary_file(image_path, make_tiff_with_datetime("2014:03:10 12:00:00"));

    {
        ImageRenameMetadataService service(temp_dir.string());
        const std::string actual = service.enrich_suggested_name(image_path, "black_ducks_row.tiff");

        CHECK(actual == "2014-03-10_black_ducks_row.tiff");
    }
    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("ImageRenameMetadataService extracts TIFF capture date")
{
    const auto temp_dir = make_temp_dir("tiff_date");
    const auto image_path = temp_dir / "sample.tiff";
    write_binary_file(image_path, make_tiff_with_datetime("2016:11:04 09:30:00"));

    {
        ImageRenameMetadataService service(temp_dir.string());
        const auto date = service.extract_capture_date(image_path);

        REQUIRE(date.has_value());
        CHECK(*date == "2016-11-04");
    }
    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("ImageRenameMetadataService reads PNG eXIf date metadata for rename prefix")
{
    const auto temp_dir = make_temp_dir("png");
    const auto image_path = temp_dir / "sample.png";
    const auto tiff_payload = make_tiff_with_datetime("2021:09:17 08:45:30");
    write_binary_file(image_path, make_png_with_exif_chunk(tiff_payload));

    {
        ImageRenameMetadataService service(temp_dir.string());
        const std::string actual = service.enrich_suggested_name(image_path, "street_market.png");

        CHECK(actual == "2021-09-17_street_market.png");
    }
    std::filesystem::remove_all(temp_dir);
}

#if !defined(_WIN32)
TEST_CASE("ImageRenameMetadataService reads HEIC date metadata via exiftool fallback")
{
    const auto temp_dir = make_temp_dir("heic");
    const auto image_path = temp_dir / "sample.heic";
    write_binary_file(image_path, std::vector<uint8_t>{0x00, 0x00, 0x00, 0x00});

    const auto exiftool_path = temp_dir / "exiftool";
    {
        std::ofstream script(exiftool_path, std::ios::binary | std::ios::trunc);
        REQUIRE(script.good());
        script << "#!/bin/sh\n";
        script << "printf '[{\"DateTimeOriginal\":\"2019:07:11 14:20:00\"}]\\n'\n";
        REQUIRE(script.good());
    }
    std::filesystem::permissions(exiftool_path,
                                 std::filesystem::perms::owner_exec |
                                 std::filesystem::perms::group_exec |
                                 std::filesystem::perms::others_exec,
                                 std::filesystem::perm_options::add);

    PathGuard path_guard;
    const std::string old_path = std::getenv("PATH") ? std::getenv("PATH") : "";
    const std::string new_path = temp_dir.string() + ":" + old_path;
    set_path_value(new_path);

    {
        ImageRenameMetadataService service(temp_dir.string());
        const std::string actual = service.enrich_suggested_name(image_path, "trip.heic");

        CHECK(actual == "2019-07-11_trip.heic");
    }
    std::filesystem::remove_all(temp_dir);
}
#endif


TEST_CASE("Geocoding skips images without GPS and falls back to dates while offline", "[geolocation]")
{
    QtAppContext qt;
    TempDir config, photos;
    ChatCompletionTestServer server;
    EnvVarGuard endpoint("AI_FILE_SORTER_NOMINATIM_URL", server.url());
    const bool has_gps = GENERATE(false, true);
    const auto image = photos.path() / "photo.tiff";
    if (has_gps) {
        ImageMetadataTestFixtures::write_gps_tiff(image, 10.1234567, 20.7654321);
    } else {
        write_binary_file(image, make_tiff_with_datetime("2020:01:02 03:04:05"));
    }
    int checks = 0;
    ImageRenameMetadataService service(config.path().string(), {
        .allow_online_place_lookup = true,
        .network_available = [&] { ++checks; return false; }});
    CHECK(server.run([&] { return service.enrich_suggested_name(image, "photo.tiff"); })
          == "2020-01-02_photo.tiff");
    CHECK(checks == (has_gps ? 1 : 0));
    CHECK(server.requests.empty());
}
