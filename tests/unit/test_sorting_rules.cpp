#include "DocumentTextAnalyzer.hpp"
#include "ILLMClient.hpp"
#include "LlavaImageAnalyzer.hpp"
#include "MainApp.hpp"
#include "MainAppTestAccess.hpp"
#include "Settings.hpp"
#include "SortingRulesDialog.hpp"
#include "TestHelpers.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <fstream>

namespace {
class FilenameReplyClient final : public ILLMClient {
public:
    std::string last_prompt;
    std::string categorize_file(const std::string&, const std::string&, FileType,
                                const std::string&) override { return {}; }
    std::string complete_prompt(const std::string& prompt, int) override
    {
        last_prompt = prompt;
        return R"({"summary":"Quarterly revenue forecast and budget planning.","filename":"quarterly_revenue_forecast_budget_planning_chart"})";
    }
    void set_prompt_logging_enabled(bool) override {}
};
}

TEST_CASE("Filename word limits default independently and persist within bounds", "[sorting-rules]")
{
    TempDir temp;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    Settings settings;
    CHECK(settings.get_image_filename_words() == 3);
    CHECK(settings.get_document_filename_words() == 3);
    settings.set_image_filename_words(6);
    settings.set_document_filename_words(4);
    REQUIRE(settings.save());
    Settings reloaded;
    REQUIRE(reloaded.load());
    CHECK(reloaded.get_image_filename_words() == 6);
    CHECK(reloaded.get_document_filename_words() == 4);
    reloaded.set_image_filename_words(-1);
    reloaded.set_document_filename_words(99);
    CHECK(reloaded.get_image_filename_words() == 3);
    CHECK(reloaded.get_document_filename_words() == 6);

    const auto path = std::filesystem::path(settings.get_config_dir()) / "config.ini";
    QSettings config(QString::fromStdString(path.string()), QSettings::IniFormat);
    config.setValue(QStringLiteral("Settings/ImageFilenameWords"), QStringLiteral("not-a-number"));
    config.setValue(QStringLiteral("Settings/DocumentFilenameWords"), 99);
    config.sync();
    Settings invalid;
    REQUIRE(invalid.load());
    CHECK(invalid.get_image_filename_words() == 3);
    CHECK(invalid.get_document_filename_words() == 6);
}

TEST_CASE("Image filename prompts and cleanup honor three through six words", "[sorting-rules]")
{
    const int words = GENERATE(3, 4, 5, 6);
    const auto policy = GENERATE(VisualPromptPolicy::LegacyLlava,
                                 VisualPromptPolicy::StructuredVisionInstruct);
    const auto prompt = LlavaImageAnalyzerTestAccess::filename_user_prompt(policy, "A quarterly revenue chart", words);
    const std::string marker = policy == VisualPromptPolicy::LegacyLlava ? "maximum of " : "maximum ";
    CHECK(prompt.find(marker + std::to_string(words) + " words") != std::string::npos);
    const std::vector<std::string> expected{
        "quarterly_revenue_forecast", "quarterly_revenue_forecast_budget",
        "quarterly_revenue_forecast_budget_planning", "quarterly_revenue_forecast_budget_planning_notes"};
    CHECK(LlavaImageAnalyzerTestAccess::sanitize_filename(
        "quarterly_revenue_forecast_budget_planning_notes", words) == expected.at(words - 3));
    CHECK(LlavaImageAnalyzerTestAccess::sanitize_filename("test_chart_001290", words) == "test_chart_001290");
}

TEST_CASE("Document filename prompts and cleanup honor three through six words", "[sorting-rules]")
{
    const int words = GENERATE(3, 4, 5, 6);
    TempDir temp;
    const auto path = temp.path() / "report.txt";
    std::ofstream(path) << "Quarterly revenue forecast and budget planning.";
    DocumentTextAnalyzer::Settings settings;
    settings.max_filename_words = words;
    DocumentTextAnalyzer analyzer(settings);
    FilenameReplyClient client;
    const auto result = analyzer.analyze(path, client);
    CHECK(client.last_prompt.find("max " + std::to_string(words) + " words") != std::string::npos);
    const std::vector<std::string> expected{
        "quarterly_revenue_forecast.txt", "quarterly_revenue_forecast_budget.txt",
        "quarterly_revenue_forecast_budget_planning.txt", "quarterly_revenue_forecast_budget_planning_chart.txt"};
    CHECK(result.suggested_name == expected.at(words - 3));
    CHECK(result.summary == "Quarterly revenue forecast and budget planning.");
}

TEST_CASE("Sorting rules opens from both entry points and retains existing controls", "[sorting-rules]")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt;
    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    Settings stored;
    stored.set_image_filename_words(5);
    stored.set_document_filename_words(4);
    REQUIRE(stored.save());
    Settings settings;
    MainApp window(settings, false);
    auto* button = window.findChild<QPushButton*>(QStringLiteral("sortingRulesButton"));
    auto* action = window.findChild<QAction*>(QStringLiteral("sortingRulesAction"));
    auto* dialog = window.findChild<QDialog*>(QStringLiteral("sortingRulesDialog"));
    REQUIRE(button);
    REQUIRE(action);
    REQUIRE(dialog);
    CHECK(MainAppTestAccess::settings_menu(window)->actions().front() == action);
    window.show();
    QApplication::processEvents();
    auto* categorize = MainAppTestAccess::categorize_files_checkbox(window);
    REQUIRE(categorize);
    CHECK_FALSE(categorize->isVisible());
    button->click();
    QApplication::processEvents();
    CHECK(dialog->isVisible());
    CHECK(categorize->isVisible());
    CHECK(dialog->isAncestorOf(categorize));
    auto* images = dialog->findChild<QSlider*>(QStringLiteral("imageFilenameWordsSlider"));
    auto* documents = dialog->findChild<QSlider*>(QStringLiteral("documentFilenameWordsSlider"));
    REQUIRE(images);
    REQUIRE(documents);
    CHECK(images->value() == 5);
    CHECK(documents->value() == 4);
    CHECK(images->minimum() == 3);
    CHECK(images->maximum() == 6);
    CHECK(documents->minimum() == 3);
    CHECK(documents->maximum() == 6);
    images->setValue(6);
    documents->setValue(4);
    CHECK(settings.get_image_filename_words() == 6);
    CHECK(settings.get_document_filename_words() == 4);
    dialog->close();
    Settings reloaded;
    REQUIRE(reloaded.load());
    CHECK(reloaded.get_image_filename_words() == 6);
    CHECK(reloaded.get_document_filename_words() == 4);
    settings.set_document_filename_words(5);
    action->trigger();
    QApplication::processEvents();
    CHECK(dialog->isVisible());
    CHECK(images->value() == 6);
    CHECK(documents->value() == 5);
    dialog->close();
    MainAppTestAccess::set_analysis_in_progress(window, true);
    CHECK_FALSE(button->isEnabled());
    CHECK_FALSE(action->isEnabled());
    MainAppTestAccess::set_analysis_in_progress(window, false);
    CHECK(button->isEnabled());
    CHECK(action->isEnabled());
}
