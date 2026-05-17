#include <catch2/catch_test_macros.hpp>

#include "CategoryLanguageSupport.hpp"
#include "MainApp.hpp"
#include "MainAppTestAccess.hpp"
#include "Settings.hpp"
#include "TestHelpers.hpp"
#include "TranslationManager.hpp"

#include <QAction>
#include <QCoreApplication>
#include <QMenu>
#include <QStringList>
#include <QTranslator>

#include <algorithm>
#include <vector>

#ifndef _WIN32
namespace {

void collect_menu_languages(QMenu* menu, std::vector<CategoryLanguage>& languages)
{
    REQUIRE(menu != nullptr);
    const QList<QAction*> actions = menu->actions();
    for (QAction* const action : actions) {
        if (!action || !action->isVisible()) {
            continue;
        }
        if (QMenu* const submenu = action->menu()) {
            collect_menu_languages(submenu, languages);
            continue;
        }
        if (!action->data().isValid()) {
            continue;
        }
        languages.push_back(static_cast<CategoryLanguage>(action->data().toInt()));
    }
}

void collect_menu_labels(QMenu* menu, QStringList& labels)
{
    REQUIRE(menu != nullptr);
    const QList<QAction*> actions = menu->actions();
    for (QAction* const action : actions) {
        if (!action || !action->isVisible()) {
            continue;
        }
        if (QMenu* const submenu = action->menu()) {
            collect_menu_labels(submenu, labels);
            continue;
        }
        labels.push_back(action->text());
    }
}

QAction* find_category_language_action(QMenu* menu, CategoryLanguage language)
{
    REQUIRE(menu != nullptr);
    const QList<QAction*> actions = menu->actions();
    for (QAction* const action : actions) {
        if (!action) {
            continue;
        }
        if (QMenu* const submenu = action->menu()) {
            if (QAction* const nested = find_category_language_action(submenu, language)) {
                return nested;
            }
            continue;
        }
        if (action && action->data().toInt() == static_cast<int>(language)) {
            return action;
        }
    }
    return nullptr;
}

std::vector<CategoryLanguage> visible_category_languages(MainApp& app)
{
    std::vector<CategoryLanguage> languages;
    QMenu* const menu = MainAppTestAccess::category_language_menu(app);
    collect_menu_languages(menu, languages);
    return languages;
}

QStringList visible_category_language_labels(MainApp& app)
{
    QStringList labels;
    QMenu* const menu = MainAppTestAccess::category_language_menu(app);
    collect_menu_labels(menu, labels);
    return labels;
}

std::vector<CategoryLanguage> sorted_by_enum(std::vector<CategoryLanguage> languages)
{
    std::sort(languages.begin(),
              languages.end(),
              [](CategoryLanguage lhs, CategoryLanguage rhs) {
                  return static_cast<int>(lhs) < static_cast<int>(rhs);
              });
    return languages;
}

class PostfixMnemonicTranslator final : public QTranslator {
public:
    void set_swedish_translation(const QString& translation)
    {
        swedish_translation_ = translation;
    }

    QString translate(const char* context,
                      const char* source_text,
                      const char* disambiguation = nullptr,
                      int n = -1) const override
    {
        Q_UNUSED(disambiguation);
        Q_UNUSED(n);
        if (QString::fromLatin1(context) == QStringLiteral("UiTranslator")
            && QString::fromLatin1(source_text) == QStringLiteral("&Swedish")) {
            return swedish_translation_;
        }
        return {};
    }

private:
    QString swedish_translation_;
};

class TranslatorInstallGuard {
public:
    explicit TranslatorInstallGuard(QTranslator& translator)
        : translator_(translator)
    {
        QCoreApplication::installTranslator(&translator_);
    }

    ~TranslatorInstallGuard()
    {
        QCoreApplication::removeTranslator(&translator_);
    }

private:
    QTranslator& translator_;
};

} // namespace

TEST_CASE("Gemma 3 category language menu exposes the full supported list")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    Settings settings;
    REQUIRE(settings.save());

    MainApp window(settings, /*development_mode=*/false);
    settings.set_llm_choice(LLMChoice::Local_4b_Gemma);

    MainAppTestAccess::refresh_category_language_menu(window);

    CHECK(sorted_by_enum(visible_category_languages(window))
          == sorted_by_enum(
              supported_category_languages_for_llm_choice(LLMChoice::Local_4b_Gemma)));
}

TEST_CASE("Category language menu follows Mistral 7B support")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    Settings settings;
    REQUIRE(settings.save());

    MainApp window(settings, /*development_mode=*/false);
    settings.set_llm_choice(LLMChoice::Local_7b);

    MainAppTestAccess::refresh_category_language_menu(window);

    CHECK(sorted_by_enum(visible_category_languages(window))
          == sorted_by_enum(
              supported_category_languages_for_llm_choice(LLMChoice::Local_7b)));
}

TEST_CASE("English-only local models force the category language menu back to English")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    Settings settings;
    REQUIRE(settings.save());

    MainApp window(settings, /*development_mode=*/false);
    settings.set_category_language(CategoryLanguage::French);
    settings.set_llm_choice(LLMChoice::Local_7b_Gemma);

    MainAppTestAccess::refresh_category_language_menu(window);

    CHECK(settings.get_category_language() == CategoryLanguage::English);
    CHECK(visible_category_languages(window)
          == supported_category_languages_for_llm_choice(LLMChoice::Local_7b_Gemma));

    QAction* const english_action =
        find_category_language_action(MainAppTestAccess::category_language_menu(window),
                                      CategoryLanguage::English);
    REQUIRE(english_action != nullptr);
    CHECK(english_action->isVisible());
    CHECK(english_action->isChecked());
}

TEST_CASE("Custom local models expose the full category language menu")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    Settings settings;
    REQUIRE(settings.save());

    MainApp window(settings, /*development_mode=*/false);
    settings.set_llm_choice(LLMChoice::Custom);

    MainAppTestAccess::refresh_category_language_menu(window);

    CHECK(sorted_by_enum(visible_category_languages(window))
          == sorted_by_enum(
              supported_category_languages_for_llm_choice(LLMChoice::Custom)));
}

TEST_CASE("Category language menu keeps visible entries alphabetized")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    Settings settings;
    REQUIRE(settings.save());

    settings.set_language(Language::French);
    TranslationManager::instance().set_language(Language::French);

    MainApp window(settings, /*development_mode=*/false);
    settings.set_llm_choice(LLMChoice::Local_4b_Gemma);

    MainAppTestAccess::trigger_retranslate(window);

    const QStringList labels = visible_category_language_labels(window);
    QStringList sorted_labels = labels;
    std::sort(sorted_labels.begin(),
              sorted_labels.end(),
              [](const QString& lhs, const QString& rhs) {
                  return QString::localeAwareCompare(lhs, rhs) < 0;
              });

    CHECK(labels == sorted_labels);
}

TEST_CASE("Category language labels strip postfix interface mnemonics")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt_context;

    PostfixMnemonicTranslator translator;
    TranslatorInstallGuard translator_guard(translator);

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    Settings settings;
    REQUIRE(settings.save());

    MainApp window(settings, /*development_mode=*/false);
    settings.set_llm_choice(LLMChoice::Local_4b_Gemma);

    translator.set_swedish_translation(QStringLiteral("瑞典文(&W)"));
    MainAppTestAccess::trigger_retranslate(window);

    QAction* const swedish_action = find_category_language_action(
        MainAppTestAccess::category_language_menu(window),
        CategoryLanguage::Swedish);
    REQUIRE(swedish_action != nullptr);
    CHECK(swedish_action->text() == QStringLiteral("瑞典文"));

    translator.set_swedish_translation(QStringLiteral("瑞典文 (&W)"));
    MainAppTestAccess::trigger_retranslate(window);
    CHECK(swedish_action->text() == QStringLiteral("瑞典文"));
}

TEST_CASE("Full Gemma 3 category language menus are compartmentalized into submenus")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    Settings settings;
    REQUIRE(settings.save());

    MainApp window(settings, /*development_mode=*/false);
    settings.set_llm_choice(LLMChoice::Local_4b_Gemma);

    MainAppTestAccess::refresh_category_language_menu(window);

    QMenu* const menu = MainAppTestAccess::category_language_menu(window);
    REQUIRE(menu != nullptr);

    bool has_submenu = false;
    for (QAction* const action : menu->actions()) {
        if (action && action->menu()) {
            has_submenu = true;
            break;
        }
    }

    CHECK(has_submenu);
}
#endif
