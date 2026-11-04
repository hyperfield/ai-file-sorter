/**
 * @file test_cyrillic_support.cpp
 * @brief Regression tests for non-ASCII (Cyrillic) user profiles, folders, file names
 *        and LLM responses, plus the Russian interface language.
 */
#include <catch2/catch_test_macros.hpp>

#include "CategorizationResponseParser.hpp"
#include "FileScanner.hpp"
#include "ImageRenameMetadataService.hpp"
#include "IniConfig.hpp"
#include "Language.hpp"
#include "LocalFsProvider.hpp"
#include "LocalLLMResponseSanitizer.hpp"
#include "Settings.hpp"
#include "TestHelpers.hpp"
#include "TranslationManager.hpp"
#include "UserLearningStore.hpp"
#include "Utils.hpp"

#include <QCoreApplication>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

// u8 literals are char8_t in C++20; keep everything as UTF-8 std::string.
std::string u8s(const char8_t* text)
{
    return std::string(reinterpret_cast<const char*>(text));
}

std::filesystem::path p(const std::string& utf8)
{
    return Utils::utf8_to_path(utf8);
}

void write_file(const std::filesystem::path& path, const std::string& content = "data")
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << content;
}

} // namespace

TEST_CASE("UTF-8 path helpers round-trip Cyrillic names") {
    const std::string name = u8s(u8"Документы/Отчёт за 2026 год — итог.pdf");
    const std::filesystem::path path = p(name);
    CHECK(Utils::path_to_utf8(path.filename()) == u8s(u8"Отчёт за 2026 год — итог.pdf"));
    CHECK(Utils::path_to_utf8(path.parent_path()) == u8s(u8"Документы"));
    CHECK(Utils::path_to_utf8(path) == name);
}

TEST_CASE("getenv_utf8 returns Cyrillic environment values intact") {
    TempDir temp_dir;
    const std::string value = Utils::path_to_utf8(temp_dir.path() / p(u8s(u8"Пользователь Иван")));
    EnvVarGuard guard("AIFS_TEST_CYRILLIC_ENV", value);

    const auto read_back = Utils::getenv_utf8("AIFS_TEST_CYRILLIC_ENV");
    REQUIRE(read_back.has_value());
    CHECK(*read_back == value);
    CHECK_FALSE(Utils::getenv_utf8("AIFS_TEST_SURELY_UNSET_VARIABLE").has_value());
}

TEST_CASE("Settings work inside a Cyrillic profile directory") {
    TempDir temp_dir;
    const auto profile_root = temp_dir.path() / p(u8s(u8"Профиль Иванова"));
    std::filesystem::create_directories(profile_root);
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", Utils::path_to_utf8(profile_root));

    const std::string sort_folder = Utils::path_to_utf8(temp_dir.path() / p(u8s(u8"Загрузки")));
    {
        Settings settings;
        const auto config_dir = p(settings.get_config_dir());
        CHECK(config_dir == profile_root / "AIFileSorter");
        CHECK(std::filesystem::is_directory(config_dir));
        settings.set_sort_folder(sort_folder);
        REQUIRE(settings.save());
        CHECK(std::filesystem::exists(config_dir / "config.ini"));
    }

    Settings reloaded;
    REQUIRE(reloaded.load());
    CHECK(reloaded.get_sort_folder() == sort_folder);
}

TEST_CASE("IniConfig saves and loads a Cyrillic path with Cyrillic values") {
    TempDir temp_dir;
    const auto ini_path = temp_dir.path() / p(u8s(u8"настройки")) / "config.ini";
    std::filesystem::create_directories(ini_path.parent_path());

    IniConfig config;
    config.setValue("Main", "Folder", u8s(u8"Мои документы"));
    REQUIRE(config.save(Utils::path_to_utf8(ini_path)));

    IniConfig loaded;
    REQUIRE(loaded.load(Utils::path_to_utf8(ini_path)));
    CHECK(loaded.getValue("Main", "Folder", "") == u8s(u8"Мои документы"));
}

TEST_CASE("UserLearningStore creates its database under a Cyrillic config directory") {
    TempDir temp_dir;
    const auto config_dir = temp_dir.path() / p(u8s(u8"Конфигурация"));
    std::filesystem::create_directories(config_dir);

    {
        UserLearningStore store(Utils::path_to_utf8(config_dir));
    }
    CHECK(std::filesystem::exists(config_dir / "user_learning.db"));
}

TEST_CASE("FileScanner reports Cyrillic folders and files as UTF-8") {
    TempDir temp_dir;
    const auto root = temp_dir.path() / p(u8s(u8"Входящие"));
    write_file(root / p(u8s(u8"Счёт №17 от июня.pdf")));
    write_file(root / p(u8s(u8"фото_отпуск.jpg")));

    FileScanner scanner;
    const auto entries = scanner.get_directory_entries(Utils::path_to_utf8(root), FileScanOptions::Files);
    REQUIRE(entries.size() == 2);

    std::vector<std::string> names;
    for (const auto& entry : entries) {
        names.push_back(entry.file_name);
        CHECK(std::filesystem::exists(p(entry.full_path)));
    }
    std::sort(names.begin(), names.end());
    CHECK(std::find(names.begin(), names.end(), u8s(u8"Счёт №17 от июня.pdf")) != names.end());
    CHECK(std::find(names.begin(), names.end(), u8s(u8"фото_отпуск.jpg")) != names.end());
}

TEST_CASE("LocalFsProvider moves files into Cyrillic category folders") {
    TempDir temp_dir;
    const auto source = temp_dir.path() / p(u8s(u8"Входящие")) / p(u8s(u8"договор аренды.docx"));
    const auto destination = temp_dir.path() / p(u8s(u8"Документы")) / p(u8s(u8"Договоры"))
                             / p(u8s(u8"договор аренды.docx"));
    write_file(source, "contract");
    std::filesystem::create_directories(destination.parent_path());

    LocalFsProvider provider;
    const auto result = provider.move_entry(Utils::path_to_utf8(source), Utils::path_to_utf8(destination));
    REQUIRE(result.success);
    CHECK_FALSE(std::filesystem::exists(source));
    CHECK(std::filesystem::exists(destination));
}

TEST_CASE("LLM responses with Cyrillic labels are parsed") {
    SECTION("inline pair after chatter") {
        const std::string raw = u8s(u8"Конечно! Вот ответ:\nДокументы : Счета");
        CHECK(LocalLLMResponseSanitizer::sanitize_categorization_output(raw) == u8s(u8"Документы : Счета"));
    }
    SECTION("Russian field labels") {
        const std::string raw = u8s(u8"Категория: Фотографии\nПодкатегория: Отпуск");
        CHECK(LocalLLMResponseSanitizer::sanitize_categorization_output(raw) == u8s(u8"Фотографии : Отпуск"));

        const auto [category, subcategory] = CategorizationResponseParser::split_category_subcategory(raw);
        CHECK(category == u8s(u8"Фотографии"));
        CHECK(subcategory == u8s(u8"Отпуск"));
    }
    SECTION("Cyrillic text ending in a dotted abbreviation is not mistaken for a file name") {
        const auto [category, subcategory] =
            CategorizationResponseParser::split_category_subcategory(u8s(u8"Разное и т.д : Прочее"));
        CHECK_FALSE(category.empty());
    }
}

TEST_CASE("Translated Russian taxonomy responses are parsed in common LLM shapes") {
    const std::string expected_category = u8s(u8"Юридические документы");
    const std::string expected_subcategory = u8s(u8"Договоры аренды");
    const std::string json =
        u8s(u8"{\"category\":\"Юридические документы\",\"subcategory\":\"Договоры аренды\"}");

    const std::vector<std::string> responses = {
        json,
        "```json\n" + json + "\n```",
        "```\n" + json + "\n```",
        u8s(u8"Вот перевод:\n") + json,
        u8s(u8"category: Юридические документы\nsubcategory: Договоры аренды"),
        u8s(u8"Юридические документы , subcategory Договоры аренды"),
    };
    for (const auto& response : responses) {
        INFO(response);
        const auto parsed = CategorizationResponseParser::parse_translated_category_response(response);
        REQUIRE(parsed.has_value());
        CHECK(parsed->first == expected_category);
        CHECK(parsed->second == expected_subcategory);
    }
}

TEST_CASE("Label length limits count characters instead of UTF-8 bytes") {
    // 50 Cyrillic letters = 100 UTF-8 bytes; must be accepted like 50 Latin letters.
    std::string long_russian;
    for (int i = 0; i < 50; ++i) {
        long_russian += u8s(u8"ж");
    }
    CHECK(CategorizationResponseParser::validate_labels(long_russian, u8s(u8"Подкатегория")).valid);

    std::string too_long;
    for (int i = 0; i < 81; ++i) {
        too_long += u8s(u8"ж");
    }
    CHECK_FALSE(CategorizationResponseParser::validate_labels(too_long, u8s(u8"Подкатегория")).valid);
}

TEST_CASE("UTF-8 truncation never splits a Cyrillic letter") {
    const std::string word = u8s(u8"Привет"); // 6 letters, 12 bytes
    for (std::size_t limit = 0; limit <= word.size() + 2; ++limit) {
        std::string copy = word;
        Utils::utf8_truncate(copy, limit);
        INFO(limit);
        CHECK(copy.size() <= limit);
        CHECK(copy.size() % 2 == 0); // only whole 2-byte letters remain
        CHECK(word.compare(0, copy.size(), copy) == 0);
    }

    std::string broken_tail = u8s(u8"Отчёт");
    broken_tail.pop_back(); // leave half of the last letter, as a fixed-size read may
    Utils::utf8_truncate(broken_tail, broken_tail.size());
    CHECK(broken_tail == u8s(u8"Отчё"));

    CHECK(Utils::utf8_boundary_at_or_after(word, 1) == 2);
    CHECK(Utils::utf8_boundary_at_or_after(word, 2) == 2);
    CHECK(Utils::utf8_length(word) == 6);
}

TEST_CASE("Image place prefixes keep Cyrillic letters") {
    CHECK(ImageRenameMetadataService::apply_prefix_to_filename(
              u8s(u8"закат.jpg"), std::string("2026-06-01"), u8s(u8"Москва, Россия"))
          == u8s(u8"2026-06-01_москва_россия_закат.jpg"));
    CHECK(ImageRenameMetadataService::apply_prefix_to_filename(
              "sunset.jpg", std::nullopt, std::string("New York"))
          == "new_york_sunset.jpg");
}

TEST_CASE("Russian interface language is available") {
    CHECK(languageFromString("ru") == Language::Russian);
    CHECK(languageFromString("Russian") == Language::Russian);
    CHECK(languageToString(Language::Russian) == QStringLiteral("Russian"));

    QtAppContext context;
    auto& manager = TranslationManager::instance();
    manager.set_language(Language::Russian);
    CHECK(manager.current_language() == Language::Russian);
    CHECK(QCoreApplication::translate("UiTranslator", "&File") == QString::fromUtf8(u8s(u8"&Файл").c_str()));
    CHECK(QCoreApplication::translate("UiTranslator", "Analyze folder")
          == QString::fromUtf8(u8s(u8"Анализировать папку").c_str()));
    // Standard dialog buttons come from Qt's own qtbase catalog (when bundled).
    const QString cancel = QCoreApplication::translate("QPlatformTheme", "Cancel");
    if (cancel != QStringLiteral("Cancel")) {
        CHECK(cancel == QString::fromUtf8(u8s(u8"Отмена").c_str()));
    } else {
        WARN("qtbase_ru.qm is not bundled in this build; standard buttons stay in English");
    }
    manager.set_language(Language::English);
    CHECK(QCoreApplication::translate("QPlatformTheme", "Cancel") == QStringLiteral("Cancel"));
}
