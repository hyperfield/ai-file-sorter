#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "CategorizationResponseParser.hpp"
#include "LocalLLMResponseSanitizer.hpp"
#include "RemoteApiError.hpp"
#include "ReviewNameValidator.hpp"
#include "Settings.hpp"
#include "UserLearningStore.hpp"
#include "Utils.hpp"
#include "TestHelpers.hpp"
#include "AnalysisRuntimeLock.hpp"
#include "CacheMaintenanceService.hpp"
#include "DatabaseManager.hpp"
#include "HeadlessReviewApplyService.hpp"
#include "LocalFsProvider.hpp"
#include "Logger.hpp"
#include "NativeRotatingFileSink.hpp"
#include "TextEncoding.hpp"
#include "UndoManager.hpp"
#include "VisualModelCatalog.hpp"
#include "AnalysisProgressTranslation.hpp"
#include "LLMSelectionDialog.hpp"
#include "SuitabilityBenchmarkDialog.hpp"
#include "LLMDownloader.hpp"
#include "ChatCompletionTestServer.hpp"
#include "ImageRenameMetadataService.hpp"
#include <QUrl>
#include <QUrlQuery>

#include <QDialogButtonBox>
#include <QPushButton>
#include <fstream>
#include <iterator>

#include <clocale>
#ifdef _WIN32
#include <windows.h>
#include <mbctype.h>
#endif

namespace {
struct LocaleRestore {
    std::string previous{std::setlocale(LC_ALL, nullptr)};
    ~LocaleRestore() { std::setlocale(LC_ALL, previous.c_str()); }
};
}

TEST_CASE("Translated taxonomy accepts wrapped JSON and collapsed pairs", "[unicode-boundaries]")
{
    const auto response = GENERATE(std::string(R"({"category":"Logiciels","subcategory":"Navigateurs"})"),
        std::string("```json\n{\"category\":\"Logiciels\",\"subcategory\":\"Navigateurs\"}\n```"),
        std::string("Voici la traduction :\n{\"category\":\"Logiciels\",\"subcategory\":\"Navigateurs\"}"),
        std::string("Logiciels , subcategory Navigateurs"));
    CAPTURE(response);
    const auto labels = CategorizationResponseParser::parse_translated_category_response(response);
    REQUIRE(labels);
    CHECK(labels->first == "Logiciels");
    CHECK(labels->second == "Navigateurs");
}

TEST_CASE("Loose taxonomy pairs preserve Unicode letters", "[unicode-boundaries]")
{
    const auto pair = GENERATE(std::pair<std::string, std::string>{"文档", "合同"},
        std::pair<std::string, std::string>{"Документы", "Счета"});
    const auto response = pair.first + " : " + pair.second;
    CHECK(CategorizationResponseParser::split_category_subcategory(response) == pair);
    CHECK(LocalLLMResponseSanitizer::sanitize_categorization_output("Here is the answer:\n" + response) == response);
}

TEST_CASE("Category length accepts fifty Unicode letters", "[unicode-boundaries]")
{
    const auto letter = GENERATE(std::string("a"), std::string("é"), std::string("Ж"), std::string("文"));
    std::string category;
    for (int i = 0; i < 50; ++i) category += letter;
    std::string error;
    CHECK(CategorizationResponseParser::validate_labels(category, "Other").valid);
    CHECK(ReviewNameValidator::validate_labels(category, "Other", error));
}

TEST_CASE("Retry After numbers use the protocol decimal separator in French", "[unicode-boundaries]")
{
    LocaleRestore restore;
#ifdef _WIN32
    REQUIRE(std::setlocale(LC_ALL, "French_France.1252"));
    INFO("ACP=" << GetACP() << " CRT multibyte CP=" << _getmbcp() << " locale=" << std::setlocale(LC_ALL, nullptr));
#else
    if (!std::setlocale(LC_ALL, "fr_FR.UTF-8")) SKIP("French locale is unavailable");
#endif
    CHECK(RemoteApiError::parse_retry_after_seconds("1.25") == 2);
    CHECK_FALSE(RemoteApiError::parse_retry_after_seconds("1,25"));
    EnvVarGuard threshold("BENCH_CAT_LOCAL_QWEN35_4B_OPTIMAL_MAX", "1.25");
    CHECK(SuitabilityBenchmarkDialogTestAccess::thresholds_for_testing(LLMChoice::Local_4b_Qwen35, false).first == 1.25);
}

#ifdef _WIN32
TEST_CASE("Wide Unicode configuration and learning database use exact native paths", "[unicode-boundaries]")
{
    TempDir temp;
    const auto root = temp.path() / L"profil été Иван";
    const wchar_t* previous = _wgetenv(L"AI_FILE_SORTER_CONFIG_DIR");
    const std::optional<std::wstring> saved = previous ? std::make_optional(std::wstring(previous)) : std::nullopt;
    struct Restore {
        std::optional<std::wstring> value;
        ~Restore() { _wputenv_s(L"AI_FILE_SORTER_CONFIG_DIR", value ? value->c_str() : L""); }
    } restore{saved};
    REQUIRE(_wputenv_s(L"AI_FILE_SORTER_CONFIG_DIR", root.c_str()) == 0);
    Settings settings;
    CHECK(Utils::utf8_to_path(settings.get_config_dir()) == root / "AIFileSorter");
    settings.set_language(Language::French);
    REQUIRE(settings.save());
    CHECK(std::filesystem::is_regular_file(root / "AIFileSorter" / "config.ini"));
    Settings reloaded;
    REQUIRE(reloaded.load());
    CHECK(reloaded.get_language() == Language::French);
    UserLearningStore store(Utils::path_to_utf8(root / "apprentissage"));
    REQUIRE(store.is_open());
    CHECK(std::filesystem::is_regular_file(root / "apprentissage" / "user_learning.db"));
}
#endif

TEST_CASE("Taxonomy JSON extraction validates types and bounded escaped objects", "[unicode-boundaries]")
{
    for (const auto& invalid : {R"({"category":[],"subcategory":"Other"})",
          R"({"category":"Docs","subcategory":{}})", R"({"category":null})",
          R"({"category":"Docs" )", "NO_MATCH", "Here is some explanatory text\nwithout a pair"}) {
        CHECK_FALSE(CategorizationResponseParser::parse_translated_category_response(invalid));
    }
    const auto escaped = CategorizationResponseParser::parse_translated_category_response(
        R"(Voici: {"category":"Dossiers {importants} privés","subcategory":"Notes \"privées\""} Fin.)");
    REQUIRE(escaped);
    CHECK(escaped->first == "Dossiers {importants} privés");
    CHECK(escaped->second == "Notes privées");
    CHECK_FALSE(CategorizationResponseParser::parse_translated_category_response(std::string(65537, 'a')));
    CHECK_FALSE(CategorizationResponseParser::parse_translated_category_response(
        "{\"category\":" + std::string(100, '[') + "0" + std::string(100, ']') + "}"));
    for (const auto depth : {62, 63, 64, 65}) {
        CHECK_FALSE(CategorizationResponseParser::parse_translated_category_response(
            "{\"category\":" + std::string(depth, '[') + "0" + std::string(depth, ']') + "}"));
    }
    CHECK_FALSE(TextEncoding::contains_letter("☁️ 😀"));
    CHECK_FALSE(TextEncoding::contains_letter(std::string("\xff\xd0", 2)));
    CHECK(TextEncoding::contains_letter("𐐀"));
}

TEST_CASE("Label limits count graphemes and retain native component limits", "[unicode-boundaries]")
{
    const auto unit = GENERATE(std::string("é"), std::string("文"), std::string("e\xcc\x81"));
    std::string label;
    for (int i = 0; i < 80; ++i) label += unit;
    std::string error;
    CHECK(CategorizationResponseParser::validate_labels(label, "Other").valid);
    CHECK(ReviewNameValidator::validate_labels(label, "Other", error));
    CHECK_FALSE(CategorizationResponseParser::validate_labels(label + unit, "Other").valid);
    CHECK_FALSE(ReviewNameValidator::validate_labels(label + unit, "Other", error));
    CHECK_FALSE(TextEncoding::label_within_limits(std::string("\xc0\xaf", 2)));
    std::string supplementary;
    for (int i = 0; i < 60; ++i) supplementary += "𐐀";
    CHECK(TextEncoding::label_within_limits(supplementary));
    CHECK_FALSE(TextEncoding::label_within_limits(std::string(256, 'a')));
    std::string oversized_cluster = "a";
    for (int i = 0; i < 255; ++i) oversized_cluster += "\xcc\x81";
    CHECK_FALSE(TextEncoding::label_within_limits(oversized_cluster));
    for (int i = 0; i < 20; ++i) supplementary += "𐐀";
#ifdef _WIN32
    CHECK(TextEncoding::label_within_limits(supplementary));
#else
    CHECK_FALSE(TextEncoding::label_within_limits(supplementary));
#endif
    for (const char* unsafe : {".", "..", "CON", "con.1", "NUL.123", "Docs/Else", "Docs\\Else"}) {
        CHECK_FALSE(CategorizationResponseParser::validate_labels(unsafe, "Other").valid);
        CHECK_FALSE(ReviewNameValidator::validate_labels(unsafe, "Other", error));
    }
}

TEST_CASE("Legacy Russian byte locale cannot corrupt UTF-8 taxonomy text", "[unicode-boundaries]")
{
    LocaleRestore restore;
#ifdef _WIN32
    REQUIRE(std::setlocale(LC_ALL, "Russian_Russia.1251"));
    INFO("ACP=" << GetACP() << " CRT multibyte CP=" << _getmbcp() << " locale=" << std::setlocale(LC_ALL, nullptr));
#else
    std::setlocale(LC_ALL, "C");
#endif
    CHECK(CategorizationResponseParser::split_category_subcategory("Документы : Счета") ==
        std::pair<std::string, std::string>{"Документы", "Счета"});
    CHECK(TextEncoding::place_slug("МОСКВА München 東京") == "москва_münchen_東京");
    CHECK(TextEncoding::place_slug("Cafe\xcc\x81 भारत") == "café_भारत");
    CHECK(RemoteApiError::parse_retry_after_seconds("1.25") == 2);
}

TEST_CASE("Unicode environment overrides survive config persistence and model discovery", "[unicode-boundaries]")
{
    TempDir temp;
    const auto root = temp.path() / Utils::utf8_to_path("été Иван 東京");
    EnvVarGuard config("AI_FILE_SORTER_CONFIG_DIR", Utils::path_to_utf8(root));
    EnvVarGuard storage("AI_FILE_SORTER_LLM_STORAGE_DIR", Utils::path_to_utf8(root / Utils::utf8_to_path("modèles")));
    const auto previous = Utils::get_llm_storage_directory_override();
    struct Restore { std::string value; ~Restore() { Utils::set_llm_storage_directory_override(value); } } restore{previous};
    Utils::set_llm_storage_directory_override("");
    REQUIRE(Utils::get_default_llm_destination() == Utils::path_to_utf8(root / Utils::utf8_to_path("modèles")));
    CHECK(Utils::utf8_to_path(Utils::make_default_path_to_file_from_download_url("https://example.test/poids.gguf")) == root / Utils::utf8_to_path("modèles") / "poids.gguf");
    Settings settings;
    settings.set_llm_storage_dir(Utils::path_to_utf8(root / Utils::utf8_to_path("persisté")));
    REQUIRE(settings.save());
    Utils::set_llm_storage_directory_override("");
    Settings reloaded;
    REQUIRE(reloaded.load());
    CHECK(Utils::get_default_llm_destination() == Utils::path_to_utf8(root / Utils::utf8_to_path("persisté")));
    const auto& backend = visual_model_descriptors().front();
    const auto& artifact = backend.artifacts.front();
    const auto path = visual_artifact_storage_path(backend, artifact);
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary).write("GGUF\0\0\0\0", 8);
    CHECK(resolve_visual_artifact_path(backend, artifact, "https://example.test/model.gguf") == path);
    EnvVarGuard profile("USERPROFILE", Utils::path_to_utf8(root));
    EnvVarGuard home("HOME", Utils::path_to_utf8(root));
    CHECK(Utils::abbreviate_user_path(Utils::path_to_utf8(root / "Documents")) == "Documents");
}

TEST_CASE("Native Unicode log sink writes rotates and flushes exact filenames", "[unicode-boundaries]")
{
    TempDir temp;
    const auto path = temp.path() / Utils::utf8_to_path("été Иван 東京") / "core.log";
    auto sink = std::make_shared<NativeRotatingFileSink>(path, 64, 2);
    spdlog::logger logger("unicode-log", sink);
    logger.set_pattern("%v");
    logger.info("{}", std::string(60, 'a'));
    logger.flush();
    logger.info("Unicode München 東京 Москва");
    logger.flush();
    CHECK(std::filesystem::is_regular_file(path.parent_path() / "core.1.log"));
    std::ifstream stream(path, std::ios::binary);
    const std::string contents{std::istreambuf_iterator<char>(stream), {}};
    CHECK(contents.find("Unicode München 東京 Москва") != std::string::npos);
}

TEST_CASE("Logger initialization write and cleanup preserve a wide Unicode profile", "[unicode-boundaries]")
{
    TempDir temp;
    const auto profile = temp.path() / Utils::utf8_to_path("été Иван 東京");
    EnvVarGuard appdata("APPDATA", Utils::path_to_utf8(profile));
    EnvVarGuard cache("XDG_CACHE_HOME", Utils::path_to_utf8(profile));
    struct Restore {
        std::vector<std::shared_ptr<spdlog::logger>> loggers;
        std::shared_ptr<spdlog::logger> default_logger = spdlog::default_logger();
        Restore() {
            for (const char* name : {"core_logger", "db_logger", "ui_logger"}) {
                if (auto logger = spdlog::get(name)) loggers.push_back(logger);
                spdlog::drop(name);
            }
        }
        ~Restore() {
            for (const char* name : {"core_logger", "db_logger", "ui_logger"}) spdlog::drop(name);
            spdlog::set_default_logger(default_logger);
            for (const auto& logger : loggers) {
                if (!spdlog::get(logger->name())) spdlog::register_logger(logger);
            }
        }
    } restore;
    Logger::setup_loggers(Logger::ConsoleOutput::Stderr);
    const auto logs = Utils::utf8_to_path(Logger::get_log_directory());
    const auto core = Logger::get_logger("core_logger");
    REQUIRE(core);
    core->info("Unicode München 東京 Москва");
    core->flush();
    {
        std::ifstream stream(logs / "core.log", std::ios::binary);
        const std::string contents{std::istreambuf_iterator<char>(stream), {}};
        CHECK(contents.find("Unicode München 東京 Москва") != std::string::npos);
    }
    const auto obsolete = logs / Utils::utf8_to_path("ancien_Счета.log");
    std::ofstream(obsolete) << "old";
    REQUIRE(Logger::clear_logs());
    CHECK_FALSE(std::filesystem::exists(obsolete));
    CHECK(std::filesystem::file_size(logs / "core.log") == 0);
    core->info("after cleanup");
    core->flush();
    CHECK(std::filesystem::file_size(logs / "core.log") > 0);
}

TEST_CASE("Model downloader writes and reloads Unicode artifacts through loopback HTTP", "[unicode-boundaries]")
{
    QtAppContext qt;
    TempDir temp;
    const auto model = temp.path() / Utils::utf8_to_path("modèles Иван") / "model.gguf";
    ChatCompletionTestServer server;
    server.respond = [](const auto&) -> ChatCompletionTestServer::Reply {
        return {200, QByteArray("GGUF\0\0\0\0", 8)};
    };
    const auto url = server.url() + "/model.gguf";
    const auto downloaded = server.run([&] {
        LLMDownloader downloader(url, Utils::path_to_utf8(model));
        downloader.init_if_needed();
        std::promise<bool> result;
        auto future = result.get_future();
        downloader.start_download({}, [&] { result.set_value(true); }, {},
            [&](const std::string&) { result.set_value(false); });
        if (future.wait_for(std::chrono::seconds(5)) != std::future_status::ready) return false;
        return future.get();
    });
    REQUIRE(downloaded);
    CHECK(std::filesystem::file_size(model) == 8);
    auto metadata = model;
    metadata += ".aifs.meta";
    CHECK(std::filesystem::is_regular_file(metadata));
    LLMDownloader reopened(url, Utils::path_to_utf8(model));
    CHECK(reopened.get_real_content_length() == 8);
    CHECK(reopened.get_local_download_status() == LLMDownloader::DownloadStatus::Complete);
}

TEST_CASE("Unicode databases locks and headless review apply undo share exact paths", "[unicode-boundaries]")
{
    QtAppContext qt;
    TempDir temp;
    const auto root = temp.path() / Utils::utf8_to_path("profil été Иван");
    std::filesystem::create_directories(root);
    EnvVarGuard filename("CATEGORIZATION_CACHE_FILE", "résultats_Счета.db");
    DatabaseManager database(Utils::path_to_utf8(root));
    CHECK(std::filesystem::is_regular_file(root / Utils::utf8_to_path("résultats_Счета.db")));
    CacheMaintenanceService maintenance(Utils::path_to_utf8(root));
    const auto info = maintenance.target_info(CacheMaintenanceTarget::Categorization);
    CHECK(info.path == root / Utils::utf8_to_path("résultats_Счета.db"));
    CHECK(info.exists);
    CHECK(info.size_bytes == 0);
    AnalysisRuntimeLock lock(root / "runtime");
    const auto lease = lock.try_acquire({.owner = AnalysisRuntimeLock::Owner::Headless, .job_id = "Unicode"});
    REQUIRE(lease);
    CHECK(lock.is_locked());
    LocalFsProvider provider;
    const auto source = root / Utils::utf8_to_path("contrat_Счета.txt");
    std::ofstream(source) << "Unicode content";
    std::string category;
    for (int i = 0; i < 50; ++i) category += "文";
    CategorizedFile entry{Utils::path_to_utf8(root), Utils::path_to_utf8(source.filename()), FileType::File, category, "Contrats"};
    HeadlessReviewApplyService service(&database, provider, nullptr);
    HeadlessReviewApplyService::Options options;
    options.base_dir = Utils::path_to_utf8(root);
    options.undo_dir = Utils::path_to_utf8(root / "undo");
    options.apply_changes = false;
    CHECK(service.apply({entry}, options).planned_count == 1);
    CHECK(std::filesystem::exists(source));
    options.apply_changes = true;
    const auto result = service.apply({entry}, options);
    CHECK(result.moved_count == 1);
    CHECK(result.undo_plan_saved);
    const auto destination = root / Utils::utf8_to_path(category) / "Contrats" / source.filename();
    CHECK(std::filesystem::is_regular_file(destination));
    UndoManager undo(options.undo_dir);
    const auto plan = undo.latest_plan_path();
    REQUIRE(plan);
    CHECK(undo.undo_plan(*plan).restored == 1);
    CHECK(std::filesystem::is_regular_file(source));
}

TEST_CASE("String EXIF coordinates use invariant parsing with a Unicode helper path", "[unicode-boundaries][geolocation]")
{
    QtAppContext qt;
    LocaleRestore restore;
#ifdef _WIN32
    REQUIRE(std::setlocale(LC_ALL, "French_France.1252"));
#else
    if (!std::setlocale(LC_ALL, "fr_FR.UTF-8")) SKIP("French locale is unavailable");
#endif
    TempDir temp;
    const auto root = temp.path() / Utils::utf8_to_path("photos été Иван");
    const auto tools = root / "tools";
    std::filesystem::create_directories(tools);
    const auto image = root / Utils::utf8_to_path("東京.heic");
    std::ofstream(image) << "synthetic HEIF fallback";
    const std::string json = R"([{"DateTimeOriginal":"2020:01:02 03:04:05","GPSLatitude":"10.1234567","GPSLongitude":"20.7654321"}])";
#ifdef _WIN32
    std::ofstream(tools / "exiftool.cmd") << "@echo off\nif not exist \"%~9\" exit /b 2\necho " << json << "\n";
    const auto separator = ";";
#else
    const auto helper = tools / "exiftool";
    std::ofstream(helper) << "#!/bin/sh\nfor last in \"$@\"; do :; done\n[ -f \"$last\" ] || exit 2\nprintf '%s\\n' '" << json << "'\n";
    std::filesystem::permissions(helper, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
    const auto separator = ":";
#endif
    EnvVarGuard path("PATH", Utils::path_to_utf8(tools) + separator + Utils::getenv_utf8("PATH").value_or(""));
    ChatCompletionTestServer server;
    EnvVarGuard endpoint("AI_FILE_SORTER_NOMINATIM_URL", server.url());
    server.respond = [](const auto&) -> ChatCompletionTestServer::Reply { return {200, R"({"address":{"city":"Paris"}})"}; };
    ImageRenameMetadataService service(Utils::path_to_utf8(root / "config"), {true, [] { return true; }});
    CHECK(server.run([&] { return service.enrich_suggested_name(image, "photo.heic"); }) == "2020-01-02_paris_photo.heic");
    REQUIRE(server.requests.size() == 1);
    const auto target = server.requests.front().headers.split('\n').front().split(' ').at(1);
    const QUrlQuery query(QUrl(QStringLiteral("http://localhost") + QString::fromUtf8(target)));
    CHECK(query.queryItemValue("lat") == "10.123");
    CHECK(query.queryItemValue("lon") == "20.765");
}

TEST_CASE("Bundled startup progress and standard button translations change and reset", "[unicode-boundaries]")
{
    QtAppContext qt;
    auto& translations = TranslationManager::instance();
    struct Reset { ~Reset() { TranslationManager::instance().set_language(Language::English); } } reset;
    translations.initialize_for_app(qApp, Language::French);
    CHECK(QCoreApplication::translate("LLMSelectionDialog", "Choose LLM Mode") == QString::fromUtf8("Choisir le mode LLM"));
    TempDir temp;
    EnvVarGuard config("AI_FILE_SORTER_CONFIG_DIR", Utils::path_to_utf8(temp.path()));
    Settings settings;
    settings.set_language(Language::French);
    LLMSelectionDialog startup(settings);
    CHECK(startup.windowTitle() == QString::fromUtf8("Choisir le mode LLM"));
    CHECK(AnalysisProgressTranslation::translate("[DOC] Analyzing %1") == QString::fromUtf8("[DOC] Analyse de %1"));
    CHECK(AnalysisProgressTranslation::translate("[DONE] No files to categorize.") == QString::fromUtf8("[TERMINÉ] Aucun fichier à catégoriser."));
    QDialogButtonBox buttons(QDialogButtonBox::Cancel | QDialogButtonBox::Save);
    CHECK(buttons.button(QDialogButtonBox::Cancel)->text() == "Annuler");
    translations.initialize_for_app(qApp, Language::French);
    translations.set_language(Language::German);
    QCoreApplication::processEvents();
    CHECK(buttons.button(QDialogButtonBox::Cancel)->text() == "Abbrechen");
    translations.set_language(Language::English);
    QCoreApplication::processEvents();
    CHECK(buttons.button(QDialogButtonBox::Cancel)->text() == "Cancel");
    for (const auto& language : translations.available_languages()) {
        if (language.id == Language::English) continue;
        QString code = language.code;
        code.replace(QLatin1Char('-'), QLatin1Char('_'));
        QTranslator catalog;
        REQUIRE(catalog.load(QStringLiteral(":/i18n/qtbase_%1.qm").arg(code)));
        for (const char* button : {"OK", "Save", "Save All", "Open", "&Yes", "Yes to &All", "&No", "N&o to All",
                                   "Abort", "Retry", "Ignore", "Close", "Cancel", "Discard", "Help", "Apply", "Reset", "Restore Defaults"}) {
            CAPTURE(code, button);
            CHECK_FALSE(catalog.translate("QPlatformTheme", button).isEmpty());
        }
        translations.set_language(language.id);
        QCoreApplication::processEvents();
        CHECK(buttons.button(QDialogButtonBox::Cancel)->text() != "Cancel");
    }
}
