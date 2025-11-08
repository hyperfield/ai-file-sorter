#include <catch2/catch_test_macros.hpp>
#include <QApplication>
#include "MainApp.hpp"
#include "MainAppTestAccess.hpp"
#include "TestHelpers.hpp"
#include "TranslationManager.hpp"
#include "Language.hpp"

namespace {

class QtAppContext {
public:
    QtAppContext() {
        if (!QApplication::instance()) {
            static int argc = 1;
            static char arg0[] = "tests";
            static char* argv[] = {arg0, nullptr};
            static QApplication* app = new QApplication(argc, argv);
            Q_UNUSED(app);
        }
    }
};

} // namespace

TEST_CASE("MainApp retranslate reflects language changes") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
    QtAppContext qt_context;

    Settings settings;
    settings.load();

    MainApp window(settings, /*development_mode=*/false);
    settings.set_language(Language::English);
    TranslationManager::instance().set_language(Language::English);
    MainAppTestAccess::trigger_retranslate(window);

    const auto english_text = MainAppTestAccess::analyze_button_text(window);
    CAPTURE(english_text);
    REQUIRE(MainAppTestAccess::analyze_button_text(window) == QStringLiteral("Analyze folder"));

    settings.set_language(Language::French);
    TranslationManager::instance().set_language(Language::French);
    MainAppTestAccess::trigger_retranslate(window);

    REQUIRE(MainAppTestAccess::analyze_button_text(window) == QStringLiteral("Analyser le dossier"));
    REQUIRE(MainAppTestAccess::path_label_text(window) == QStringLiteral("Dossier :"));
}
