#include <catch2/catch_test_macros.hpp>

#include "HeadlessSettingsOverridesJson.hpp"
#include "HeadlessAnalysisWorkflowHost.hpp"
#include "Settings.hpp"
#include "TestHelpers.hpp"
#include "Utils.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <string>
#include <fstream>
#include <iterator>
#include <limits>
#include <utility>
#include <vector>

/** @brief Inspects effective in-memory preferences without exposing mutable host state. */
class HeadlessAnalysisWorkflowHostTestAccess {
public:
    static std::pair<int, int> filename_words(const HeadlessAnalysisWorkflowHost& host)
    {
        return {host.settings_.get_image_filename_words(), host.settings_.get_document_filename_words()};
    }
    static SortingMode sorting_mode(const HeadlessAnalysisWorkflowHost& host)
    {
        return host.settings_.get_sorting_mode();
    }
};

TEST_CASE("HeadlessSettingsOverridesJson parses direct whitelist lists")
{
    QJsonArray categories;
    categories.append(QStringLiteral("Documents"));
    categories.append(QStringLiteral(" Finance "));
    categories.append(QString());
    categories.append(42);

    QJsonArray subcategories;
    subcategories.append(QStringLiteral("Invoices"));
    subcategories.append(QStringLiteral(" Contracts "));

    QJsonObject object;
    object.insert(QStringLiteral("useWhitelist"), true);
    object.insert(QStringLiteral("sortingMode"), QStringLiteral("existing-folder-tree"));
    object.insert(QStringLiteral("destinationFolder"),
                  QString::fromUtf8(" C:/Archive/\xC3\x89t\xC3\xA9 "));
    object.insert(QStringLiteral("suggestNewFolders"), true);
    object.insert(QStringLiteral("activeWhitelist"), QStringLiteral("Live LLM Whitelist"));
    object.insert(QStringLiteral("allowedCategories"), categories);
    object.insert(QStringLiteral("allowedSubcategories"), subcategories);

    const HeadlessSettingsOverrides overrides = HeadlessSettingsOverridesJson::from_json(object);

    REQUIRE(overrides.use_whitelist.has_value());
    CHECK(*overrides.use_whitelist);
    REQUIRE(overrides.sorting_mode.has_value());
    CHECK(*overrides.sorting_mode == SortingMode::ExistingFolderTree);
    REQUIRE(overrides.destination_folder.has_value());
    CHECK(*overrides.destination_folder == std::string("C:/Archive/") + "\xC3\x89t\xC3\xA9");
    REQUIRE(overrides.suggest_new_folders.has_value());
    CHECK(*overrides.suggest_new_folders);
    REQUIRE(overrides.active_whitelist.has_value());
    CHECK(*overrides.active_whitelist == "Live LLM Whitelist");
    REQUIRE(overrides.allowed_categories.has_value());
    CHECK(*overrides.allowed_categories == std::vector<std::string>{"Documents", "Finance"});
    REQUIRE(overrides.allowed_subcategories.has_value());
    CHECK(*overrides.allowed_subcategories == std::vector<std::string>{"Invoices", "Contracts"});
}

TEST_CASE("HeadlessSettingsOverridesJson parses bounded independent filename word limits", "[headless][sorting-rules]")
{
    for (int words = 3; words <= 6; ++words) {
        const auto overrides = HeadlessSettingsOverridesJson::from_json(
            {{QStringLiteral("imageFilenameWords"), words}, {QStringLiteral("documentFilenameWords"), 9 - words}});
        REQUIRE(overrides.image_filename_words.has_value());
        REQUIRE(overrides.document_filename_words.has_value());
        CHECK(*overrides.image_filename_words == words);
        CHECK(*overrides.document_filename_words == 9 - words);
    }
    for (double words : {-1e100, 0.0, 2.0, 7.0, 1e100}) {
        const auto overrides = HeadlessSettingsOverridesJson::from_json(
            {{QStringLiteral("imageFilenameWords"), words}, {QStringLiteral("documentFilenameWords"), words}});
        REQUIRE(overrides.image_filename_words.has_value());
        CHECK(*overrides.image_filename_words == (words < 3 ? 3 : 6));
        CHECK(overrides.document_filename_words == overrides.image_filename_words);
    }
    const auto absent = HeadlessSettingsOverridesJson::from_json({});
    CHECK_FALSE(absent.image_filename_words.has_value());
    CHECK_FALSE(absent.document_filename_words.has_value());
    const std::vector<QJsonValue> invalid{QJsonValue(), QJsonValue(true), QJsonValue(QStringLiteral("6")),
        QJsonValue(4.5), QJsonValue(QJsonArray{}), QJsonValue(QJsonObject{}),
        QJsonValue(std::numeric_limits<double>::infinity()), QJsonValue(std::numeric_limits<double>::quiet_NaN())};
    for (const auto& value : invalid) {
        const auto overrides = HeadlessSettingsOverridesJson::from_json(
            {{QStringLiteral("imageFilenameWords"), value}, {QStringLiteral("documentFilenameWords"), 6}});
        CHECK_FALSE(overrides.image_filename_words.has_value());
        CHECK(overrides.document_filename_words == 6);
        const auto document = HeadlessSettingsOverridesJson::from_json(
            {{QStringLiteral("documentFilenameWords"), value}});
        CHECK_FALSE(document.document_filename_words.has_value());
    }
}

TEST_CASE("HeadlessAnalysisWorkflowHost isolates filename word overrides from persisted preferences", "[headless][sorting-rules]")
{
    TempDir temp;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    Settings stored;
    stored.set_image_filename_words(4);
    stored.set_document_filename_words(5);
    REQUIRE(stored.save());
    const auto config_path = std::filesystem::path(stored.get_config_dir()) / "config.ini";
    const auto read_config = [&]() {
        std::ifstream input(config_path, std::ios::binary);
        REQUIRE(input.good());
        return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    };
    const auto before = read_config();
    HeadlessAnalysisWorkflowHost::Options options;
    options.folder_path = temp.path();
    std::pair<int, int> expected{4, 5};
    SECTION("absent overrides inherit both saved limits") {}
    SECTION("image override leaves the saved document limit in use") {
        options.settings_overrides.image_filename_words = 6;
        expected = {6, 5};
    }
    SECTION("document override leaves the saved image limit in use") {
        options.settings_overrides.document_filename_words = 3;
        expected = {4, 3};
    }
    SECTION("both overrides replace the saved values independently") {
        options.settings_overrides = HeadlessSettingsOverridesJson::from_json(
            {{QStringLiteral("imageFilenameWords"), 6}, {QStringLiteral("documentFilenameWords"), 3}});
        expected = {6, 3};
    }
    SECTION("direct in-memory overrides are bounded too") {
        options.settings_overrides.image_filename_words = -10;
        options.settings_overrides.document_filename_words = 100;
        expected = {3, 6};
    }
    {
        HeadlessAnalysisWorkflowHost host(std::move(options));
        CHECK(HeadlessAnalysisWorkflowHostTestAccess::filename_words(host) == expected);
        CHECK(read_config() == before);
    }
    CHECK(read_config() == before);
    Settings reloaded;
    REQUIRE(reloaded.load());
    CHECK(reloaded.get_image_filename_words() == 4);
    CHECK(reloaded.get_document_filename_words() == 5);
}

TEST_CASE("HeadlessAnalysisWorkflowHost resets destination while inheriting sorting preferences", "[headless][sorting-rules]")
{
    TempDir temp;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    const auto source = temp.path() / "inbox";
    const auto archive = temp.path() / "archive";
    std::filesystem::create_directories(source);
    std::filesystem::create_directories(archive);
    Settings stored;
    stored.set_destination_folder(Utils::path_to_utf8(archive));
    stored.set_sorting_mode(SortingMode::ExistingFolderTree);
    stored.set_suggest_new_folders(true);
    REQUIRE(stored.save());
    const auto config = std::filesystem::path(stored.get_config_dir()) / "config.ini";
    const auto config_bytes = [&]() {
        std::ifstream input(config, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    };
    const auto before = config_bytes();
    HeadlessAnalysisWorkflowHost::Options options;
    options.folder_path = Utils::path_to_utf8(source);
    auto destination = source;
    auto mode = SortingMode::ExistingFolderTree;
    bool suggest_new = true;
    SECTION("omitted overrides inherit mode and permission but reset destination") {}
    SECTION("empty destination explicitly selects the analyzed root") {
        options.settings_overrides.destination_folder = "";
    }
    SECTION("an explicit override selects the archive") {
        options.settings_overrides.destination_folder = Utils::path_to_utf8(archive);
        destination = archive;
    }
    SECTION("an independent extension can override inherited sorting") {
        options.settings_overrides.sorting_mode = SortingMode::GeneratedCategories;
        options.settings_overrides.suggest_new_folders = false;
        mode = SortingMode::GeneratedCategories;
        suggest_new = false;
    }
    {
        HeadlessAnalysisWorkflowHost host(std::move(options));
        CHECK(std::filesystem::equivalent(Utils::utf8_to_path(host.destination_folder()), destination));
        CHECK(HeadlessAnalysisWorkflowHostTestAccess::sorting_mode(host) == mode);
        CHECK(host.suggest_new_folders() == suggest_new);
        CHECK(config_bytes() == before);
    }
    CHECK(config_bytes() == before);
    Settings reloaded;
    REQUIRE(reloaded.load());
    CHECK(reloaded.get_sorting_mode() == SortingMode::ExistingFolderTree);
    CHECK(reloaded.get_suggest_new_folders());
    CHECK(reloaded.get_destination_folder() == Utils::path_to_utf8(archive));
}
