#include <catch2/catch_test_macros.hpp>

#include "HeadlessSettingsOverridesJson.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <string>
#include <vector>

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
