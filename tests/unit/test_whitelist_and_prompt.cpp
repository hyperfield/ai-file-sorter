#include <catch2/catch_test_macros.hpp>

#include "CategorizationService.hpp"
#include "CategorizationServiceTestAccess.hpp"
#include "DatabaseManager.hpp"
#include "CategoryLanguage.hpp"
#include "Settings.hpp"
#include "WhitelistStore.hpp"
#include "TestHelpers.hpp"

TEST_CASE("WhitelistStore initializes from settings and persists defaults") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    settings.set_allowed_categories({"Alpha", "Beta"});
    settings.set_allowed_subcategories({"One", "Two"});
    settings.set_active_whitelist(std::string());

    WhitelistStore store(settings.get_config_dir());
    store.initialize_from_settings(settings);

    auto names = store.list_names();
    REQUIRE(names.size() == 1);
    REQUIRE(names.front() == store.default_name());
    auto entry = store.get(store.default_name());
    REQUIRE(entry.has_value());
    REQUIRE(entry->categories == std::vector<std::string>{"Alpha", "Beta"});
    REQUIRE(entry->subcategories == std::vector<std::string>{"One", "Two"});

    REQUIRE(settings.get_active_whitelist() == store.default_name());
    REQUIRE(settings.get_allowed_categories() == entry->categories);
    REQUIRE(settings.get_allowed_subcategories() == entry->subcategories);

    WhitelistStore reloaded(settings.get_config_dir());
    REQUIRE(reloaded.load());
    auto persisted = reloaded.get(store.default_name());
    REQUIRE(persisted.has_value());
    REQUIRE(persisted->categories == entry->categories);
    REQUIRE(persisted->subcategories == entry->subcategories);
}

TEST_CASE("CategorizationService builds numbered whitelist context") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    settings.set_allowed_categories({"CatA", "CatB"});
    settings.set_allowed_subcategories({});
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    const std::string context = CategorizationServiceTestAccess::build_whitelist_context(service);

    REQUIRE(context.find("Allowed main categories") != std::string::npos);
    REQUIRE(context.find("1) CatA") != std::string::npos);
    REQUIRE(context.find("2) CatB") != std::string::npos);
    REQUIRE(context.find("Allowed subcategories: any") != std::string::npos);
}

TEST_CASE("CategorizationService builds category language context when non-English selected") {
    TempDir base_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", base_dir.path().string());
    Settings settings;
    settings.set_category_language(CategoryLanguage::French);
    DatabaseManager db(settings.get_config_dir());
    CategorizationService service(settings, db, nullptr);

    const std::string context = CategorizationServiceTestAccess::build_category_language_context(service);

    REQUIRE_FALSE(context.empty());
    REQUIRE(context.find("French") != std::string::npos);
}
