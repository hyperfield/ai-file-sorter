#include <catch2/catch_test_macros.hpp>
#include "MainApp.hpp"
#include "MainAppTestAccess.hpp"
#include "TestHelpers.hpp"
#include "TranslationManager.hpp"
#include "Language.hpp"

#include <vector>

#ifndef _WIN32
TEST_CASE("MainApp retranslate reflects language changes") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
    QtAppContext qt_context;

    Settings settings;
    settings.load();

    MainApp window(settings, /*development_mode=*/false);
    struct ExpectedTranslation {
        Language language;
        QString analyze_label;
        QString folder_label;
    };

    const std::vector<ExpectedTranslation> expected = {
        {Language::English, QStringLiteral("Analyze folder"), QStringLiteral("Folder:")},
        {Language::French, QStringLiteral("Analyser le dossier"), QStringLiteral("Dossier :")},
        {Language::German, QStringLiteral("Ordner analysieren"), QStringLiteral("Ordner:")},
        {Language::Italian, QStringLiteral("Analizza cartella"), QStringLiteral("Cartella:")},
        {Language::Spanish, QStringLiteral("Analizar carpeta"), QStringLiteral("Carpeta:")},
        {Language::Dutch, QStringLiteral("Map analyseren"), QStringLiteral("Map:")},
        {Language::Turkish, QStringLiteral("Klasörü analiz et"), QStringLiteral("Klasör:")},
        {Language::Korean, QStringLiteral("폴더 분석"), QStringLiteral("폴더:")}
    };

    for (const auto& entry : expected) {
        settings.set_language(entry.language);
        TranslationManager::instance().set_language(entry.language);
        MainAppTestAccess::trigger_retranslate(window);

        const auto analyze_text = MainAppTestAccess::analyze_button_text(window);
        const auto folder_text = MainAppTestAccess::path_label_text(window);
        CAPTURE(static_cast<int>(entry.language), analyze_text, folder_text);

        REQUIRE(analyze_text == entry.analyze_label);
        REQUIRE(folder_text == entry.folder_label);
    }
}
#endif
