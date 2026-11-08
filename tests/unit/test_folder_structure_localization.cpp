#include "FolderStructureInitializerDialog.hpp"
#include "FolderStructureLocalizationService.hpp"
#include "FolderStructureTemplates.hpp"
#include "ILLMClient.hpp"
#include "MainApp.hpp"
#include "MainAppTestAccess.hpp"
#include "Settings.hpp"
#include "TestHelpers.hpp"
#include "Utils.hpp"

#include <QApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QThread>
#include <QTimer>
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <functional>

namespace {
class TranslationLLM final : public ILLMClient {
public:
    explicit TranslationLLM(std::function<std::string(const std::string&)> reply) : reply_(std::move(reply)) {}
    std::string categorize_file(const std::string&, const std::string&, FileType, const std::string&) override { return {}; }
    std::string complete_prompt(const std::string& prompt, int) override { return reply_(prompt); }
    void set_prompt_logging_enabled(bool) override {}
private:
    std::function<std::string(const std::string&)> reply_;
};

QJsonObject input_labels(const std::string& prompt)
{
    const auto json_start = prompt.find('{');
    if (json_start == std::string::npos) throw std::runtime_error("No translation input");
    return QJsonDocument::fromJson(QByteArray::fromStdString(prompt.substr(json_start))).object();
}

std::string translated_labels(const std::string& prompt)
{
    auto result = input_labels(prompt);
    for (auto it = result.begin(); it != result.end(); ++it) {
        it.value() = QStringLiteral("Traduit ") + it.value().toString();
    }
    return QJsonDocument(result).toJson(QJsonDocument::Compact).toStdString();
}

bool wait_until(const std::function<bool()>& ready)
{
    QElapsedTimer timer;
    timer.start();
    while (!ready() && timer.elapsed() < 3000) {
        QApplication::processEvents();
        QThread::msleep(1);
    }
    return ready();
}

FolderStructureInitializerDialog::LanguageOptions french_options()
{
    FolderStructureInitializerDialog::LanguageOptions options;
    options.initial_language = CategoryLanguage::French;
    options.llm_factory = [] { return std::make_unique<TranslationLLM>(translated_labels); };
    return options;
}
} // namespace

TEST_CASE("Folder structure translation preserves numbering and shared parent labels", "[folder-language]")
{
    FolderStructureTemplates::Descriptor source;
    source.relative_directories = {"10-19 Admin", "10-19 Admin/11 Finance", "20-29 Work",
                                   "20-29 Work/21 Reports", "20-29 Work/21 Reports/21.01 Finance", "20-29 Work/22 Finance"};
    int calls = 0;
    TranslationLLM llm([&](const std::string& prompt) {
        ++calls;
        CHECK(prompt.find("10-19") == std::string::npos);
        auto result = input_labels(prompt);
        const std::map<QString, QString> labels = {{"Admin", "Administration"}, {"Finance", "Finances"},
                                                  {"Work", "Travail"}, {"Reports", "Rapports"}};
        for (auto it = result.begin(); it != result.end(); ++it) it.value() = labels.at(it.value().toString());
        return QJsonDocument(result).toJson(QJsonDocument::Compact).toStdString();
    });
    CHECK(FolderStructureLocalizationService::localize(source, CategoryLanguage::English, llm).relative_directories == source.relative_directories);
    CHECK(calls == 0);
    const auto translated = FolderStructureLocalizationService::localize(source, CategoryLanguage::French, llm);
    CHECK(translated.relative_directories == std::vector<std::string>{"10-19 Administration", "10-19 Administration/11 Finances",
        "20-29 Travail", "20-29 Travail/21 Rapports", "20-29 Travail/21 Rapports/21.01 Finances", "20-29 Travail/22 Finances"});
    CHECK(calls == 1);
    CHECK(source.relative_directories[0] == "10-19 Admin");
}

TEST_CASE("Folder structure translation rejects unsafe incomplete and colliding names", "[folder-language]")
{
    FolderStructureTemplates::Descriptor source;
    source.relative_directories = {"Documents", "Documents/Reports", "Images"};
    for (const std::string mode : {"missing", "extra", "malformed", "not-string", "../Escape", "/absolute", "back\\slash", "CON", "trailing.", "collision", "unicode-collision"}) {
        DYNAMIC_SECTION(mode) {
            TranslationLLM llm([&](const std::string& prompt) {
                if (mode == "malformed") return std::string("not JSON");
                auto result = input_labels(prompt);
                if (mode == "missing") result.remove("0");
                else if (mode == "extra") result.insert("extra", "Extra");
                else if (mode == "not-string") result.insert("0", 42);
                else if (mode == "collision") result.insert("2", "documents");
                else if (mode == "unicode-collision") {
                    result.insert("0", QString::fromUtf8("École"));
                    result.insert("2", QString::fromUtf8("e\xcc\x81" "COLE"));
                } else result.insert("0", QString::fromStdString(mode));
                return QJsonDocument(result).toJson(QJsonDocument::Compact).toStdString();
            });
            CHECK_THROWS(FolderStructureLocalizationService::localize(source, CategoryLanguage::French, llm));
        }
    }
}

TEST_CASE("Folder structure translation stops without returning a partial template", "[folder-language]")
{
    const auto& source = *FolderStructureTemplates::find(FolderStructureTemplates::Id::MediaType);
    std::atomic<bool> stop{false};
    int calls = 0;
    TranslationLLM llm([&](const std::string& prompt) {
        ++calls;
        stop = true;
        return translated_labels(prompt);
    });
    CHECK_THROWS(FolderStructureLocalizationService::localize(source, CategoryLanguage::French, llm, &stop));
    CHECK(calls == 1);
}

TEST_CASE("Folder structure dialog creates the translated Unicode preview and caches language changes", "[folder-language]")
{
    EnvVarGuard platform("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt;
    TempDir destination;
    auto options = french_options();
    std::atomic<int> clients{0};
    options.llm_factory = [&] {
        ++clients;
        return std::make_unique<TranslationLLM>([](const auto& prompt) {
            auto result = input_labels(prompt);
            for (auto it = result.begin(); it != result.end(); ++it) it.value() = QString::fromUtf8("Éléments ") + it.value().toString();
            return QJsonDocument(result).toJson(QJsonDocument::Compact).toStdString();
        });
    };
    FolderStructureInitializerDialog dialog(destination.path(), {}, std::move(options));
    auto* language = dialog.findChild<QComboBox*>("folderStructureLanguageSelector");
    auto* preview = dialog.findChild<QListWidget*>("folderStructurePreviewList");
    auto* create = dialog.findChild<QPushButton*>("folderStructureCreateButton");
    REQUIRE(language);
    REQUIRE(preview);
    REQUIRE(create);
    CHECK(dialog.folder_language() == CategoryLanguage::French);
    CHECK_FALSE(create->isEnabled());
    REQUIRE(wait_until([&] { return create->isEnabled(); }));
    CHECK(clients == 1);
    QStringList paths;
    for (int i = 0; i < preview->count(); ++i) paths << preview->item(i)->text();
    REQUIRE_FALSE(paths.empty());
    CHECK(paths[0] == QString::fromUtf8("Éléments Documents"));
    language->setCurrentIndex(language->findData(static_cast<int>(CategoryLanguage::English)));
    CHECK(preview->item(0)->text() == "Documents");
    language->setCurrentIndex(language->findData(static_cast<int>(CategoryLanguage::French)));
    CHECK(preview->item(0)->text() == paths[0]);
    CHECK(clients == 1);
    QTimer::singleShot(0, &dialog, [] {
        if (auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) message->accept();
    });
    create->click();
    CHECK(dialog.result() == QDialog::Accepted);
    CHECK(dialog.created_count() == static_cast<std::size_t>(paths.size()));
    for (const auto& path : paths) CHECK(std::filesystem::is_directory(destination.path() / Utils::utf8_to_path(path.toStdString())));
    CHECK_FALSE(std::filesystem::exists(destination.path() / "Documents"));
}

TEST_CASE("Folder structure dialog blocks failed translations until retry succeeds", "[folder-language]")
{
    EnvVarGuard platform("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt;
    TempDir destination;
    auto options = french_options();
    std::atomic<bool> fail{true};
    options.llm_factory = [&] {
        return std::make_unique<TranslationLLM>([&](const auto& prompt) {
            return fail ? std::string("invalid") : translated_labels(prompt);
        });
    };
    FolderStructureInitializerDialog dialog(destination.path(), {}, std::move(options));
    auto* retry = dialog.findChild<QPushButton*>("folderStructureRetryTranslationButton");
    auto* create = dialog.findChild<QPushButton*>("folderStructureCreateButton");
    auto* preview = dialog.findChild<QListWidget*>("folderStructurePreviewList");
    REQUIRE(wait_until([&] { return !retry->isHidden(); }));
    CHECK_FALSE(create->isEnabled());
    CHECK(preview->count() == 0);
    CHECK(std::filesystem::is_empty(destination.path()));
    fail = false;
    retry->click();
    REQUIRE(wait_until([&] { return create->isEnabled(); }));
    CHECK(retry->isHidden());
    CHECK(preview->count() > 0);
}

TEST_CASE("Folder structure dialog drains cancellation before releasing the analysis runtime", "[folder-language]")
{
    EnvVarGuard platform("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt;
    TempDir destination, runtime;
    auto options = french_options();
    options.runtime_directory = runtime.path();
    std::atomic<bool> entered{false}, release{false};
    options.llm_factory = [&] {
        return std::make_unique<TranslationLLM>([&](const auto& prompt) {
            entered = true;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (!release && std::chrono::steady_clock::now() < deadline) QThread::msleep(1);
            return translated_labels(prompt);
        });
    };
    FolderStructureInitializerDialog dialog(destination.path(), {}, std::move(options));
    dialog.show();
    REQUIRE(wait_until([&] { return entered.load(); }));
    AnalysisRuntimeLock lock(runtime.path());
    CHECK(lock.is_locked());
    dialog.reject();
    CHECK(dialog.isVisible());
    CHECK(lock.is_locked());
    release = true;
    REQUIRE(wait_until([&] { return !dialog.isVisible(); }));
    CHECK_FALSE(lock.is_locked());
    CHECK(std::filesystem::is_empty(destination.path()));
}

TEST_CASE("Main window defaults folder language to category language without changing the preference", "[folder-language]")
{
    EnvVarGuard platform("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt;
    TempDir config;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config.path().string());
    Settings settings;
    settings.set_llm_choice(LLMChoice::Local_4b_Qwen35);
    settings.set_category_language(CategoryLanguage::French);
    REQUIRE(settings.save());
    MainApp window(settings, false);
    bool inspected = false;
    QTimer::singleShot(0, &window, [&] {
        auto* dialog = dynamic_cast<FolderStructureInitializerDialog*>(QApplication::activeModalWidget());
        CHECK(dialog != nullptr);
        if (!dialog) return;
        inspected = true;
        CHECK(dialog->folder_language() == CategoryLanguage::French);
        auto* language = dialog->findChild<QComboBox*>("folderStructureLanguageSelector");
        language->setCurrentIndex(language->findData(static_cast<int>(CategoryLanguage::English)));
        CHECK(dialog->folder_language() == CategoryLanguage::English);
        CHECK(settings.get_category_language() == CategoryLanguage::French);
        dialog->reject();
    });
    MainAppTestAccess::create_folder_structure_button(window)->click();
    CHECK(inspected);
    Settings reloaded;
    REQUIRE(reloaded.load());
    CHECK(reloaded.get_category_language() == CategoryLanguage::French);
}
