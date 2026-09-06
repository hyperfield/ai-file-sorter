#include <catch2/catch_test_macros.hpp>
#include "BulkEditDialog.hpp"
#include "CategorizationDialog.hpp"
#include "CloudCompatibilityProvider.hpp"
#include "DatabaseManager.hpp"
#include "IFilePreviewService.hpp"
#include "Language.hpp"
#include "Logger.hpp"
#include "LocalFsProvider.hpp"
#include "ReviewHistoryStore.hpp"
#include "StorageProviderRegistry.hpp"
#include "TestHooks.hpp"
#include "TestHelpers.hpp"
#include "TranslationManager.hpp"
#include "UndoManager.hpp"
#include "UserLearningStore.hpp"
#include "Utils.hpp"
#include <QAbstractItemView>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QTimer>
#include <QStandardItemModel>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace {

class RecordingPreviewService final : public IFilePreviewService {
public:
    bool preview_file(const std::filesystem::path& file_path,
                      QWidget* parent) override {
        ++calls;
        last_path = file_path;
        last_parent = parent;
        return should_succeed;
    }

    int calls{0};
    bool should_succeed{true};
    std::filesystem::path last_path;
    QWidget* last_parent{nullptr};
};

std::string normalized_path_text(std::string value)
{
    std::replace(value.begin(), value.end(), '\\', '/');
    return value;
}

} // namespace

TEST_CASE("CategorizationDialog delegates preview requests to the preview service") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    TempDir undo_dir;
    CategorizedFile file;
    file.file_path = "/tmp";
    file.file_name = "preview.txt";
    file.type = FileType::File;
    file.category = "Docs";
    file.subcategory = "Reports";

    CategorizationDialog dialog(nullptr, true, undo_dir.path().string());
    auto preview_service = std::make_unique<RecordingPreviewService>();
    auto* preview_service_ptr = preview_service.get();
    dialog.set_file_preview_service(std::move(preview_service));
    dialog.test_set_entries({file});

    REQUIRE(dialog.test_trigger_preview(0));
    REQUIRE(preview_service_ptr->calls == 1);
    CHECK(preview_service_ptr->last_path == std::filesystem::path("/tmp") / "preview.txt");
    CHECK(preview_service_ptr->last_parent == &dialog);
}

TEST_CASE("BulkEditDialog enables OK only for meaningful edits") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    BulkEditDialog dialog(true);
    const auto edits = dialog.findChildren<QLineEdit*>();
    REQUIRE(edits.size() == 2);
    auto* buttons = dialog.findChild<QDialogButtonBox*>();
    REQUIRE(buttons != nullptr);
    auto* ok_button = buttons->button(QDialogButtonBox::Ok);
    REQUIRE(ok_button != nullptr);

    CHECK_FALSE(ok_button->isEnabled());

    edits.at(0)->setText(QStringLiteral("  Reports  "));
    CHECK(ok_button->isEnabled());
    CHECK(dialog.category() == "Reports");
    CHECK(dialog.subcategory().empty());

    edits.at(0)->clear();
    CHECK_FALSE(ok_button->isEnabled());

    edits.at(1)->setText(QStringLiteral("  Quarterly  "));
    CHECK(ok_button->isEnabled());
    CHECK(dialog.category().empty());
    CHECK(dialog.subcategory() == "Quarterly");
}

TEST_CASE("BulkEditDialog omits subcategory edits when disabled") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    BulkEditDialog dialog(false);
    const auto edits = dialog.findChildren<QLineEdit*>();
    REQUIRE(edits.size() == 1);

    edits.at(0)->setText(QStringLiteral("Manuals"));
    CHECK(dialog.category() == "Manuals");
    CHECK(dialog.subcategory().empty());
}

namespace {

struct MoveProbeGuard {
    ~MoveProbeGuard() {
        TestHooks::reset_categorization_move_probe();
    }
};

void schedule_active_modal_accept()
{
    auto closer = std::make_shared<std::function<void(int)>>();
    *closer = [closer](int attempts_remaining) {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (!dialog) {
            if (attempts_remaining > 0) {
                QTimer::singleShot(0, [closer, attempts_remaining]() {
                    (*closer)(attempts_remaining - 1);
                });
            }
            return;
        }
        dialog->accept();
    };

    QTimer::singleShot(0, [closer]() {
        (*closer)(10);
    });
}

CategorizedFile sample_file() {
    CategorizedFile file;
    file.file_path = "/tmp";
    file.file_name = "example.txt";
    file.type = FileType::File;
    file.category = "Docs";
    file.subcategory = "Reports";
    return file;
}

} // namespace

TEST_CASE("CategorizationDialog uses subcategory toggle when moving files") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    const std::vector<CategorizedFile> files = {sample_file()};

    auto verify_toggle = [&](bool initial_state, bool toggled_state) {
        TempDir undo_dir;
        CategorizationDialog dialog(nullptr, initial_state, undo_dir.path().string());
        dialog.test_set_entries(files);

        bool probe_called = false;
        bool recorded_flag = !toggled_state;
        MoveProbeGuard guard;

        TestHooks::set_categorization_move_probe(
            [&](const TestHooks::CategorizationMoveInfo& info) {
                probe_called = true;
                recorded_flag = info.show_subcategory_folders;
            });

        dialog.set_show_subcategory_column(toggled_state);
        dialog.test_trigger_confirm();

        REQUIRE(probe_called);
        CHECK(recorded_flag == toggled_state);
    };

    SECTION("Enabled state honored") {
        verify_toggle(true, true);
    }

    SECTION("Disabling hides subcategory folders") {
        verify_toggle(true, false);
    }

    SECTION("Enabling from disabled state works") {
        verify_toggle(false, true);
    }
}

TEST_CASE("CategorizationDialog supports sorting by columns") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    CategorizedFile alpha;
    alpha.file_path = "/tmp";
    alpha.file_name = "b.txt";
    alpha.type = FileType::File;
    alpha.category = "Alpha";
    alpha.subcategory = "One";

    CategorizedFile beta;
    beta.file_path = "/tmp";
    beta.file_name = "a.txt";
    beta.type = FileType::File;
    beta.category = "Beta";
    beta.subcategory = "Two";

    TempDir undo_dir;
    CategorizationDialog dialog(nullptr, true, undo_dir.path().string());
    dialog.test_set_entries({alpha, beta});

    auto* table = dialog.findChild<QTableView*>();
    REQUIRE(table != nullptr);
    auto* model = qobject_cast<QStandardItemModel*>(table->model());
    REQUIRE(model != nullptr);

    SECTION("Sorts by file name ascending") {
        table->sortByColumn(1, Qt::AscendingOrder); // file name column
        REQUIRE(model->item(0, 1)->text() == QStringLiteral("a.txt"));
        REQUIRE(model->item(1, 1)->text() == QStringLiteral("b.txt"));
    }

    SECTION("Sorts by category descending") {
        table->sortByColumn(4, Qt::DescendingOrder); // category column
        REQUIRE(model->item(0, 4)->text() == QStringLiteral("Beta"));
        REQUIRE(model->item(1, 4)->text() == QStringLiteral("Alpha"));
    }
}

TEST_CASE("CategorizationDialog does not edit cells on selected click") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    TempDir undo_dir;
    CategorizationDialog dialog(nullptr, true, undo_dir.path().string());
    dialog.test_set_entries({sample_file()});

    auto* table = dialog.findChild<QTableView*>();
    REQUIRE(table != nullptr);

    const auto triggers = table->editTriggers();
    CHECK(triggers.testFlag(QAbstractItemView::DoubleClicked));
    CHECK(triggers.testFlag(QAbstractItemView::EditKeyPressed));
    CHECK_FALSE(triggers.testFlag(QAbstractItemView::SelectedClicked));
}

TEST_CASE("CategorizationDialog auto-approves rows by enabled operation") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    CategorizedFile rename_only = sample_file();
    rename_only.file_name = "rename-only.txt";
    rename_only.category.clear();
    rename_only.subcategory.clear();
    rename_only.rename_only = true;
    rename_only.suggested_name = "renamed.txt";

    CategorizedFile categorized = sample_file();
    categorized.file_name = "categorized.txt";

    CategorizedFile combined = sample_file();
    combined.file_name = "combined.txt";
    combined.suggested_name = "combined-renamed.txt";

    CategorizedFile incomplete = sample_file();
    incomplete.file_name = "incomplete.txt";
    incomplete.category.clear();
    incomplete.subcategory.clear();

    TempDir undo_dir;
    CategorizationDialog dialog(nullptr, true, undo_dir.path().string());
    dialog.set_review_auto_approve_filename_changes_enabled(true);
    dialog.test_set_entries({rename_only, categorized, combined, incomplete});

    auto* table = dialog.findChild<QTableView*>();
    REQUIRE(table != nullptr);
    auto* model = qobject_cast<QStandardItemModel*>(table->model());
    REQUIRE(model != nullptr);
    REQUIRE(model->rowCount() == 4);

    CHECK(model->item(0, 0)->checkState() == Qt::Checked);
    CHECK(model->item(1, 0)->checkState() == Qt::Unchecked);
    CHECK(model->item(2, 0)->checkState() == Qt::Unchecked);
    CHECK(model->item(3, 0)->checkState() == Qt::Unchecked);

    dialog.set_review_auto_approve_categorization_enabled(true);
    CHECK(model->item(0, 0)->checkState() == Qt::Checked);
    CHECK(model->item(1, 0)->checkState() == Qt::Checked);
    CHECK(model->item(2, 0)->checkState() == Qt::Checked);
    CHECK(model->item(3, 0)->checkState() == Qt::Unchecked);

    dialog.set_review_auto_approve_filename_changes_enabled(false);
    CHECK(model->item(0, 0)->checkState() == Qt::Unchecked);
    CHECK(model->item(1, 0)->checkState() == Qt::Checked);
    CHECK(model->item(2, 0)->checkState() == Qt::Unchecked);
    CHECK(model->item(3, 0)->checkState() == Qt::Unchecked);
}

TEST_CASE("CategorizationDialog undo restores moved files") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    TempDir temp_dir;
    const std::filesystem::path base = temp_dir.path();
    const std::string file_name = "alpha.txt";
    const std::filesystem::path source = base / file_name;
    std::ofstream(source).put('x');

    CategorizedFile file;
    file.file_path = base.string();
    file.file_name = file_name;
    file.type = FileType::File;
    file.category = "Docs";
    file.subcategory = "Reports";

    TempDir undo_dir_for_dialog;
    CategorizationDialog dialog(nullptr, true, undo_dir_for_dialog.path().string());
    dialog.test_set_entries({file});

    REQUIRE_FALSE(dialog.test_undo_enabled());

    dialog.test_trigger_confirm();

    const std::filesystem::path destination = base / file.category / file.subcategory / file_name;
    REQUIRE_FALSE(std::filesystem::exists(source));
    REQUIRE(std::filesystem::exists(destination));
    REQUIRE(dialog.test_undo_enabled());

    dialog.test_trigger_undo();

    REQUIRE(std::filesystem::exists(source));
    REQUIRE_FALSE(std::filesystem::exists(destination));
    REQUIRE_FALSE(dialog.test_undo_enabled());
}

TEST_CASE("CategorizationDialog records applied review history with descriptions")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    TempDir history_dir;
    ReviewHistoryStore history_store(history_dir.path().string());
    REQUIRE(history_store.is_open());

    TempDir temp_dir;
    const std::filesystem::path base = temp_dir.path();
    const std::string file_name = "picnic.jpg";
    const std::filesystem::path source = base / file_name;
    std::ofstream(source).put('x');

    CategorizedFile file;
    file.file_path = base.string();
    file.file_name = file_name;
    file.type = FileType::File;
    file.category = "Photos";
    file.subcategory = "Family";
    file.learning_context = "Image description: Sunny picnic near the lake.";

    TempDir undo_dir_for_dialog;
    CategorizationDialog dialog(nullptr,
                                true,
                                undo_dir_for_dialog.path().string(),
                                CategoryLanguage::English,
                                nullptr,
                                nullptr,
                                &history_store);
    dialog.test_set_entries({file});

    dialog.test_trigger_confirm();

    const std::filesystem::path destination = base / file.category / file.subcategory / file_name;
    REQUIRE_FALSE(std::filesystem::exists(source));
    REQUIRE(std::filesystem::exists(destination));

    auto entries = history_store.entries();
    REQUIRE(entries.size() == 1);
    CHECK(entries.front().operation == ReviewHistoryStore::Operation::Categorize);
    CHECK(entries.front().original_file_name == file_name);
    CHECK(entries.front().final_file_name == file_name);
    CHECK(entries.front().category == "Photos");
    CHECK(entries.front().subcategory == "Family");
    CHECK(entries.front().file_description == "Sunny picnic near the lake.");
    CHECK_FALSE(entries.front().undone);

    dialog.test_trigger_undo();
    entries = history_store.entries();
    REQUIRE(entries.size() == 1);
    CHECK(entries.front().undone);
}

TEST_CASE("CategorizationDialog keeps date category suffixes out of stored canonical categories")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    DatabaseManager db(config_dir.path().string());

    TempDir temp_dir;
    const std::filesystem::path base = temp_dir.path();
    const std::string file_name = "manual.pdf";
    const std::filesystem::path source = base / file_name;
    std::ofstream(source).put('x');

    CategorizedFile file;
    file.file_path = base.string();
    file.file_name = file_name;
    file.type = FileType::File;
    file.category = "Records_2026-06";
    file.subcategory = "Monthly Statements";
    file.canonical_category = "Records";
    file.canonical_subcategory = "Monthly Statements";

    TempDir undo_dir_for_dialog;
    CategorizationDialog dialog(&db, true, undo_dir_for_dialog.path().string());
    dialog.test_set_entries({file});

    dialog.test_trigger_confirm();

    const std::filesystem::path destination = base / file.category / file.subcategory / file_name;
    REQUIRE_FALSE(std::filesystem::exists(source));
    REQUIRE(std::filesystem::exists(destination));

    const auto cached = db.get_categorized_file(base.string(), file_name, FileType::File);
    REQUIRE(cached.has_value());
    CHECK(cached->category == "Records");
    CHECK(cached->subcategory == "Monthly Statements");
}

TEST_CASE("CategorizationDialog moves Unicode files into French nested subcategory folders")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    TempDir temp_dir;
    const std::filesystem::path base = temp_dir.path();
    const std::string file_name = "東京_보고서.txt";
    const std::filesystem::path source = base / Utils::utf8_to_path(file_name);
    std::ofstream(source).put('x');

    CategorizedFile file;
    file.file_path = Utils::path_to_utf8(base);
    file.file_name = file_name;
    file.type = FileType::File;
    file.category = "Logiciels";
    file.subcategory = "Navigateur";

    TempDir undo_dir_for_dialog;
    CategorizationDialog dialog(nullptr,
                                true,
                                undo_dir_for_dialog.path().string(),
                                CategoryLanguage::French);
    dialog.test_set_entries({file});

    auto* table = dialog.findChild<QTableView*>();
    REQUIRE(table != nullptr);
    auto* model = qobject_cast<QStandardItemModel*>(table->model());
    REQUIRE(model != nullptr);

    const auto expected_destination =
        base / Utils::utf8_to_path("Logiciels") / Utils::utf8_to_path("Navigateur") /
        Utils::utf8_to_path(file_name);
    const std::string expected_preview = normalized_path_text(Utils::path_to_utf8(expected_destination));
    auto* preview_item = model->item(0, 7);
    REQUIRE(preview_item != nullptr);
    const std::string preview =
        normalized_path_text(preview_item->text().toStdString());
    CHECK(preview == expected_preview);
    CHECK(preview.find("subcategory") == std::string::npos);

    dialog.show();
    QApplication::processEvents();
    TranslationManager::instance().set_language(Language::French);
    QEvent language_change(QEvent::LanguageChange);
    QApplication::sendEvent(&dialog, &language_change);
    QApplication::processEvents();

    table->sortByColumn(1, Qt::AscendingOrder);
    dialog.set_show_subcategory_column(false);
    dialog.set_show_subcategory_column(true);
    QApplication::processEvents();

    dialog.test_trigger_confirm();

    REQUIRE_FALSE(std::filesystem::exists(source));
    REQUIRE(std::filesystem::exists(expected_destination));
    CHECK_FALSE(std::filesystem::exists(base / Utils::utf8_to_path("Logiciels , subcategory Navigateur") /
                                        Utils::utf8_to_path(file_name)));
    CHECK(dialog.test_undo_enabled());

    for (const auto& entry : std::filesystem::recursive_directory_iterator(base)) {
        CHECK(normalized_path_text(Utils::path_to_utf8(entry.path())).find("subcategory") ==
              std::string::npos);
    }

    dialog.test_trigger_undo();

    REQUIRE(std::filesystem::exists(source));
    REQUIRE_FALSE(std::filesystem::exists(expected_destination));
    CHECK_FALSE(dialog.test_undo_enabled());
}

TEST_CASE("CategorizationDialog undo allows renaming again") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    TempDir temp_dir;
    const std::filesystem::path base = temp_dir.path();
    const std::string file_name = "photo.jpg";
    const std::string renamed = "sunset.jpg";
    const std::filesystem::path source = base / file_name;
    const std::filesystem::path destination = base / renamed;
    std::ofstream(source).put('x');

    CategorizedFile file;
    file.file_path = base.string();
    file.file_name = file_name;
    file.type = FileType::File;
    file.rename_only = true;
    file.suggested_name = renamed;

    TempDir undo_dir_for_dialog;
    CategorizationDialog dialog(nullptr, true, undo_dir_for_dialog.path().string());
    dialog.test_set_entries({file});

    dialog.test_trigger_confirm();
    REQUIRE_FALSE(std::filesystem::exists(source));
    REQUIRE(std::filesystem::exists(destination));

    dialog.test_trigger_undo();
    REQUIRE(std::filesystem::exists(source));
    REQUIRE_FALSE(std::filesystem::exists(destination));

    dialog.test_trigger_confirm();
    REQUIRE_FALSE(std::filesystem::exists(source));
    REQUIRE(std::filesystem::exists(destination));
}

TEST_CASE("CategorizationDialog dry run logs preview completion without moved success") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    TempDir cache_home;
    EnvVarGuard cache_home_guard("XDG_CACHE_HOME", cache_home.path().string());

    auto logger = Logger::get_logger("core_logger");
    if (!logger) {
        Logger::setup_loggers();
        logger = Logger::get_logger("core_logger");
    }
    REQUIRE(logger != nullptr);

    std::string clear_error;
    REQUIRE(Logger::clear_logs(&clear_error));

    TempDir temp_dir;
    const std::filesystem::path base = temp_dir.path();
    const std::string file_name = "draft.txt";
    const std::filesystem::path source = base / file_name;
    const std::filesystem::path destination = base / "Docs" / "Reports" / file_name;
    std::ofstream(source).put('x');

    CategorizedFile file;
    file.file_path = base.string();
    file.file_name = file_name;
    file.type = FileType::File;
    file.category = "Docs";
    file.subcategory = "Reports";

    TempDir undo_dir_for_dialog;
    CategorizationDialog dialog(nullptr, true, undo_dir_for_dialog.path().string());
    dialog.test_set_entries({file});

    auto* dry_run_checkbox = [&dialog]() -> QCheckBox* {
        const auto checkboxes = dialog.findChildren<QCheckBox*>();
        for (auto* checkbox : checkboxes) {
            if (checkbox && checkbox->text().contains(QStringLiteral("Dry run"))) {
                return checkbox;
            }
        }
        return nullptr;
    }();
    REQUIRE(dry_run_checkbox != nullptr);
    dry_run_checkbox->setChecked(true);

    schedule_active_modal_accept();
    dialog.test_trigger_confirm();
    logger->flush();

    REQUIRE(std::filesystem::exists(source));
    REQUIRE_FALSE(std::filesystem::exists(destination));

    const auto core_log_path = std::filesystem::path(Logger::get_log_directory()) / "core.log";
    std::ifstream log_stream(core_log_path);
    REQUIRE(log_stream.good());
    const std::string log_text((std::istreambuf_iterator<char>(log_stream)),
                               std::istreambuf_iterator<char>());

    CHECK(log_text.find("Dry run completed. No files were moved.") != std::string::npos);
    CHECK(log_text.find("All files have been sorted and moved successfully.") == std::string::npos);
}

TEST_CASE("UndoManager restores saved plans through the active storage provider") {
    TempDir undo_dir;
    TempDir data_dir;

    StorageProviderRegistry registry;
    auto local_provider = std::make_shared<LocalFsProvider>();
    registry.register_builtin(local_provider);

    const std::filesystem::path source = data_dir.path() / "original.txt";
    const std::filesystem::path destination = data_dir.path() / "Moved" / "original.txt";
    std::ofstream(source).put('x');

    const auto move_result = local_provider->move_entry(source.string(), destination.string());
    REQUIRE(move_result.success);
    REQUIRE_FALSE(std::filesystem::exists(source));
    REQUIRE(std::filesystem::exists(destination));

    UndoManager writer(undo_dir.path().string());
    REQUIRE(writer.save_plan(data_dir.path().string(),
                             local_provider->id(),
                             {UndoManager::Entry{
                                 source.string(),
                                 destination.string(),
                                 move_result.metadata.size_bytes,
                                 move_result.metadata.mtime}},
                             nullptr));

    UndoManager reader(undo_dir.path().string(), &registry);
    const auto plan_path = reader.latest_plan_path();
    REQUIRE(plan_path.has_value());

    const auto undo_result = reader.undo_plan(*plan_path);
    CHECK(undo_result.restored == 1);
    CHECK(undo_result.skipped == 0);
    REQUIRE(std::filesystem::exists(source));
    REQUIRE_FALSE(std::filesystem::exists(destination));
}

TEST_CASE("UndoManager relaxes timestamp validation for cloud providers") {
    TempDir undo_dir;
    TempDir data_dir;

    StorageProviderRegistry registry;
    auto cloud_provider = std::make_shared<CloudCompatibilityProvider>(
        "onedrive",
        "OneDrive",
        std::vector<std::string>{"onedrive"});
    registry.register_builtin(cloud_provider);

    const std::filesystem::path source = data_dir.path() / "original.txt";
    const std::filesystem::path destination = data_dir.path() / "Moved" / "original.txt";
    std::ofstream(source).put('x');

    const auto move_result = cloud_provider->move_entry(source.string(), destination.string());
    REQUIRE(move_result.success);

    std::error_code ec;
    std::filesystem::last_write_time(
        destination,
        std::filesystem::file_time_type::clock::now() + std::chrono::seconds(10),
        ec);
    REQUIRE(!ec);

    UndoManager writer(undo_dir.path().string());
    REQUIRE(writer.save_plan(data_dir.path().string(),
                             cloud_provider->id(),
                             {UndoManager::Entry{
                                 source.string(),
                                 destination.string(),
                                 move_result.metadata.size_bytes,
                                 move_result.metadata.mtime}},
                             nullptr));

    UndoManager reader(undo_dir.path().string(), &registry);
    const auto plan_path = reader.latest_plan_path();
    REQUIRE(plan_path.has_value());

    const auto undo_result = reader.undo_plan(*plan_path);
    CHECK(undo_result.restored == 1);
    CHECK(undo_result.skipped == 0);
    REQUIRE(std::filesystem::exists(source));
    REQUIRE_FALSE(std::filesystem::exists(destination));
}

TEST_CASE("CategorizationDialog rename-only updates cached filename") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    DatabaseManager db(config_dir.path().string());

    TempDir temp_dir;
    const std::filesystem::path base = temp_dir.path();
    const std::string file_name = "photo.jpg";
    const std::string renamed = "sunset.jpg";
    const std::filesystem::path source = base / file_name;
    const std::filesystem::path destination = base / renamed;
    std::ofstream(source).put('x');

    CategorizedFile file;
    file.file_path = base.string();
    file.file_name = file_name;
    file.type = FileType::File;
    file.rename_only = true;
    file.suggested_name = renamed;

    TempDir undo_dir_for_dialog;
    CategorizationDialog dialog(&db, true, undo_dir_for_dialog.path().string());
    dialog.test_set_entries({file});

    dialog.test_trigger_confirm();
    REQUIRE_FALSE(std::filesystem::exists(source));
    REQUIRE(std::filesystem::exists(destination));

    const auto old_cache = db.get_categorization_from_db(base.string(), file_name, FileType::File);
    REQUIRE(old_cache.empty());

    const auto cached = db.get_categorized_files(base.string());
    REQUIRE(cached.size() == 1);
    CHECK(cached.front().file_name == renamed);
    CHECK(cached.front().rename_only);
    CHECK(cached.front().suggested_name == renamed);
}

TEST_CASE("CategorizationDialog allows editing when rename-only checkbox is off") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    CategorizedFile rename_only_entry;
    rename_only_entry.file_path = "/tmp";
    rename_only_entry.file_name = "a.png";
    rename_only_entry.type = FileType::File;
    rename_only_entry.rename_only = true;
    rename_only_entry.suggested_name = "example.png";

    CategorizedFile categorized_entry;
    categorized_entry.file_path = "/tmp";
    categorized_entry.file_name = "b.png";
    categorized_entry.type = FileType::File;
    categorized_entry.category = "Images";
    categorized_entry.subcategory = "Screens";

    TempDir undo_dir_for_dialog;
    CategorizationDialog dialog(nullptr, false, undo_dir_for_dialog.path().string());
    dialog.test_set_entries({rename_only_entry, categorized_entry});

    auto* table = dialog.findChild<QTableView*>();
    REQUIRE(table != nullptr);
    auto* model = qobject_cast<QStandardItemModel*>(table->model());
    REQUIRE(model != nullptr);

    CHECK(model->item(0, 4)->isEditable());
    CHECK(model->item(1, 4)->isEditable());
}

TEST_CASE("CategorizationDialog deduplicates suggested names when rename-only is toggled") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    CategorizedFile first;
    first.file_path = "/tmp";
    first.file_name = "a.png";
    first.type = FileType::File;
    first.category = "Images";
    first.subcategory = "Screens";
    first.suggested_name = "computer_screen_youtube.png";

    CategorizedFile second = first;
    second.file_name = "b.png";
    second.category = "Media";

    TempDir undo_dir_for_dialog;
    CategorizationDialog dialog(nullptr, true, undo_dir_for_dialog.path().string());
    dialog.test_set_entries({first, second});

    QCheckBox* rename_checkbox = nullptr;
    const auto checkboxes = dialog.findChildren<QCheckBox*>();
    for (auto* checkbox : checkboxes) {
        if (checkbox && checkbox->text() == QStringLiteral("Do not categorize picture files (only rename)")) {
            rename_checkbox = checkbox;
            break;
        }
    }
    REQUIRE(rename_checkbox != nullptr);
    rename_checkbox->setChecked(true);

    auto* table = dialog.findChild<QTableView*>();
    REQUIRE(table != nullptr);
    auto* model = qobject_cast<QStandardItemModel*>(table->model());
    REQUIRE(model != nullptr);

    const QString first_suggestion = model->item(0, 3)->text();
    const QString second_suggestion = model->item(1, 3)->text();

    CHECK(first_suggestion == QStringLiteral("computer_screen_youtube_1.png"));
    CHECK(second_suggestion == QStringLiteral("computer_screen_youtube_2.png"));
}

TEST_CASE("CategorizationDialog avoids double suffixes for numbered suggestions") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    CategorizedFile first;
    first.file_path = "/tmp";
    first.file_name = "a.png";
    first.type = FileType::File;
    first.rename_only = true;
    first.suggested_name = "computer_screen_youtube_1.png";

    CategorizedFile second = first;
    second.file_name = "b.png";

    TempDir undo_dir_for_dialog;
    CategorizationDialog dialog(nullptr, false, undo_dir_for_dialog.path().string());
    dialog.test_set_entries({first, second});

    auto* table = dialog.findChild<QTableView*>();
    REQUIRE(table != nullptr);
    auto* model = qobject_cast<QStandardItemModel*>(table->model());
    REQUIRE(model != nullptr);

    const QString first_suggestion = model->item(0, 3)->text();
    const QString second_suggestion = model->item(1, 3)->text();

    CHECK(first_suggestion == QStringLiteral("computer_screen_youtube_1.png"));
    CHECK(second_suggestion == QStringLiteral("computer_screen_youtube_2.png"));
}

TEST_CASE("CategorizationDialog hides suggested names for renamed entries") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    CategorizedFile entry;
    entry.file_path = "/tmp";
    entry.file_name = "already_renamed.png";
    entry.type = FileType::File;
    entry.suggested_name = "new_name.png";
    entry.rename_applied = true;

    TempDir undo_dir_for_dialog;
    CategorizationDialog dialog(nullptr, false, undo_dir_for_dialog.path().string());
    dialog.test_set_entries({entry});

    auto* table = dialog.findChild<QTableView*>();
    REQUIRE(table != nullptr);
    auto* model = qobject_cast<QStandardItemModel*>(table->model());
    REQUIRE(model != nullptr);

    CHECK(model->item(0, 3)->text().isEmpty());
    CHECK_FALSE(model->item(0, 3)->isEditable());
}

TEST_CASE("CategorizationDialog hides already renamed rows when rename-only is on") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    CategorizedFile renamed;
    renamed.file_path = "/tmp";
    renamed.file_name = "renamed.png";
    renamed.type = FileType::File;
    renamed.category = "Images";
    renamed.subcategory = "Screens";
    renamed.rename_applied = true;

    CategorizedFile pending;
    pending.file_path = "/tmp";
    pending.file_name = "pending.png";
    pending.type = FileType::File;
    pending.category = "Images";
    pending.subcategory = "Screens";
    pending.suggested_name = "pending_new.png";

    TempDir undo_dir_for_dialog;
    CategorizationDialog dialog(nullptr, true, undo_dir_for_dialog.path().string());
    dialog.test_set_entries({renamed, pending});

    auto* table = dialog.findChild<QTableView*>();
    REQUIRE(table != nullptr);
    auto* model = qobject_cast<QStandardItemModel*>(table->model());
    REQUIRE(model != nullptr);

    auto find_row = [&](const QString& name) -> int {
        for (int row = 0; row < model->rowCount(); ++row) {
            if (model->item(row, 1)->text() == name) {
                return row;
            }
        }
        return -1;
    };

    const int renamed_row = find_row(QStringLiteral("renamed.png"));
    const int pending_row = find_row(QStringLiteral("pending.png"));
    REQUIRE(renamed_row >= 0);
    REQUIRE(pending_row >= 0);

    CHECK_FALSE(table->isRowHidden(renamed_row));
    CHECK_FALSE(table->isRowHidden(pending_row));

    QCheckBox* rename_checkbox = nullptr;
    const auto checkboxes = dialog.findChildren<QCheckBox*>();
    for (auto* checkbox : checkboxes) {
        if (checkbox && checkbox->text() == QStringLiteral("Do not categorize picture files (only rename)")) {
            rename_checkbox = checkbox;
            break;
        }
    }
    REQUIRE(rename_checkbox != nullptr);

    rename_checkbox->setChecked(true);
    CHECK(table->isRowHidden(renamed_row));
}

TEST_CASE("CategorizationDialog deduplicates suggested picture filenames") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    TempDir temp_dir;
    const std::filesystem::path base = temp_dir.path();

    CategorizedFile first;
    first.file_path = base.string();
    first.file_name = "a.png";
    first.type = FileType::File;
    first.suggested_name = "sunny_barbeque_dinner.png";
    first.rename_only = true;

    CategorizedFile second = first;
    second.file_name = "b.png";

    TempDir undo_dir_for_dialog;
    CategorizationDialog dialog(nullptr, false, undo_dir_for_dialog.path().string());
    dialog.test_set_entries({first, second});

    auto* table = dialog.findChild<QTableView*>();
    REQUIRE(table != nullptr);
    auto* model = qobject_cast<QStandardItemModel*>(table->model());
    REQUIRE(model != nullptr);

    const QString first_suggestion = model->item(0, 3)->text();
    const QString second_suggestion = model->item(1, 3)->text();

    CHECK(first_suggestion == QStringLiteral("sunny_barbeque_dinner_1.png"));
    CHECK(second_suggestion == QStringLiteral("sunny_barbeque_dinner_2.png"));
}

TEST_CASE("CategorizationDialog deduplicates suggested media filenames") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    TempDir temp_dir;
    const std::filesystem::path base = temp_dir.path();

    CategorizedFile first;
    first.file_path = base.string();
    first.file_name = "20250917_124208.mp4";
    first.type = FileType::File;
    first.category = "Media files";
    first.suggested_name = "2025_videohandle.mp4";

    CategorizedFile second = first;
    second.file_name = "20250917_124239.mp4";

    TempDir undo_dir_for_dialog;
    CategorizationDialog dialog(nullptr, false, undo_dir_for_dialog.path().string());
    dialog.test_set_entries({first, second});

    auto* table = dialog.findChild<QTableView*>();
    REQUIRE(table != nullptr);
    auto* model = qobject_cast<QStandardItemModel*>(table->model());
    REQUIRE(model != nullptr);

    const QString first_suggestion = model->item(0, 3)->text();
    const QString second_suggestion = model->item(1, 3)->text();

    CHECK(first_suggestion == QStringLiteral("2025_videohandle_1.mp4"));
    CHECK(second_suggestion == QStringLiteral("2025_videohandle_2.mp4"));
}

TEST_CASE("CategorizationDialog avoids existing picture filename collisions") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    TempDir temp_dir;
    const std::filesystem::path base = temp_dir.path();
    const std::string existing_name = "sunny_barbeque_dinner.png";
    std::ofstream(base / existing_name).put('x');

    CategorizedFile entry;
    entry.file_path = base.string();
    entry.file_name = "c.png";
    entry.type = FileType::File;
    entry.suggested_name = existing_name;
    entry.rename_only = true;

    TempDir undo_dir_for_dialog;
    CategorizationDialog dialog(nullptr, false, undo_dir_for_dialog.path().string());
    dialog.test_set_entries({entry});

    auto* table = dialog.findChild<QTableView*>();
    REQUIRE(table != nullptr);
    auto* model = qobject_cast<QStandardItemModel*>(table->model());
    REQUIRE(model != nullptr);

    const QString suggestion = model->item(0, 3)->text();
    CHECK(suggestion == QStringLiteral("sunny_barbeque_dinner_1.png"));
}

TEST_CASE("CategorizationDialog rename-only preserves cached categories without renaming") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    DatabaseManager db(config_dir.path().string());

    TempDir temp_dir;
    const std::filesystem::path base = temp_dir.path();
    const std::string file_name = "photo.jpg";
    std::ofstream(base / file_name).put('x');

    auto resolved = db.resolve_category("Images", "Screens");
    REQUIRE(db.insert_or_update_file_with_categorization(
        file_name, "F", base.string(), resolved, false, std::string(), false));

    CategorizedFile file;
    file.file_path = base.string();
    file.file_name = file_name;
    file.type = FileType::File;
    file.rename_only = true;

    TempDir undo_dir_for_dialog;
    CategorizationDialog dialog(&db, true, undo_dir_for_dialog.path().string());
    dialog.test_set_entries({file});

    dialog.test_trigger_confirm();

    const auto cached = db.get_categorized_file(base.string(), file_name, FileType::File);
    REQUIRE(cached.has_value());
    CHECK(cached->category == resolved.category);
    CHECK(cached->subcategory == resolved.subcategory);
    CHECK(cached->rename_only);
}

TEST_CASE("CategorizationDialog preserves cached subcategories when the subcategory column is hidden") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    DatabaseManager db(config_dir.path().string());

    TempDir temp_dir;
    const std::filesystem::path base = temp_dir.path();
    const std::string file_name = "photo.jpg";
    std::ofstream(base / file_name).put('x');

    auto resolved = db.resolve_category("Images", "Screens");
    REQUIRE(db.insert_or_update_file_with_categorization(
        file_name, "F", base.string(), resolved, false, std::string(), false));

    CategorizedFile file;
    file.file_path = base.string();
    file.file_name = file_name;
    file.type = FileType::File;
    file.category = "Images";
    file.subcategory = "Screens";

    TempDir undo_dir_for_dialog;
    CategorizationDialog dialog(&db, true, undo_dir_for_dialog.path().string());
    dialog.test_set_entries({file});
    dialog.set_show_subcategory_column(false);

    dialog.show();
    QApplication::processEvents();
    dialog.close();
    QApplication::processEvents();

    const auto cached = db.get_categorized_file(base.string(), file_name, FileType::File);
    REQUIRE(cached.has_value());
    CHECK(cached->category == resolved.category);
    CHECK(cached->subcategory == resolved.subcategory);
}

TEST_CASE("CategorizationDialog rename-only preserves cached categories when renaming") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    DatabaseManager db(config_dir.path().string());

    TempDir temp_dir;
    const std::filesystem::path base = temp_dir.path();
    const std::string file_name = "photo.jpg";
    const std::string renamed = "sunset.jpg";
    const std::filesystem::path source = base / file_name;
    const std::filesystem::path destination = base / renamed;
    std::ofstream(source).put('x');

    auto resolved = db.resolve_category("Images", "Screens");
    REQUIRE(db.insert_or_update_file_with_categorization(
        file_name, "F", base.string(), resolved, false, std::string(), false));

    CategorizedFile file;
    file.file_path = base.string();
    file.file_name = file_name;
    file.type = FileType::File;
    file.rename_only = true;
    file.suggested_name = renamed;

    TempDir undo_dir_for_dialog;
    CategorizationDialog dialog(&db, true, undo_dir_for_dialog.path().string());
    dialog.test_set_entries({file});

    dialog.test_trigger_confirm();
    REQUIRE_FALSE(std::filesystem::exists(source));
    REQUIRE(std::filesystem::exists(destination));

    const auto cached = db.get_categorized_file(base.string(), renamed, FileType::File);
    REQUIRE(cached.has_value());
    CHECK(cached->category == resolved.category);
    CHECK(cached->subcategory == resolved.subcategory);
    CHECK(cached->rename_only);
}

TEST_CASE("CategorizationDialog records confirmed categories as learned behavior") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", preferred_qt_test_platform());
    QtAppContext qt_context;

    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    DatabaseManager db(config_dir.path().string());
    UserLearningStore learning_store(config_dir.path().string());
    REQUIRE(learning_store.is_open());

    TempDir temp_dir;
    const std::filesystem::path base = temp_dir.path();
    const std::string file_name = "manual.pdf";
    std::ofstream(base / file_name).put('x');

    CategorizedFile file;
    file.file_path = base.string();
    file.file_name = file_name;
    file.type = FileType::File;
    file.category = "Manuals";
    file.subcategory = "Product Guides";
    file.learning_context = "Document summary: Setup and maintenance instructions.";

    TempDir undo_dir_for_dialog;
    CategorizationDialog dialog(&db,
                                true,
                                undo_dir_for_dialog.path().string(),
                                CategoryLanguage::English,
                                nullptr,
                                &learning_store);
    dialog.test_set_entries({file});

    dialog.test_trigger_confirm();

    CHECK(learning_store.approved_example_count() == 1);
    const auto learned = learning_store.find_taxonomy_entry("Manuals", "Product Guides");
    REQUIRE(learned.has_value());
    CHECK(learned->example_count == 1);

    const auto examples = learning_store.approved_examples();
    REQUIRE(examples.size() == 1);
    CHECK(examples.front().file_name == file_name);
    CHECK(examples.front().dir_path == base.string());
    CHECK(examples.front().category == "Manuals");
    CHECK(examples.front().subcategory == "Product Guides");
    CHECK(examples.front().context_text == file.learning_context);
}
