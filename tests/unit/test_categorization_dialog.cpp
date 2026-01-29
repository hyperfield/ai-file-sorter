#include <catch2/catch_test_macros.hpp>
#include "CategorizationDialog.hpp"
#include "DatabaseManager.hpp"
#include "TestHooks.hpp"
#include "TestHelpers.hpp"
#include <QCheckBox>
#include <QTableView>
#include <QStandardItemModel>
#include <filesystem>
#include <fstream>

#ifndef _WIN32

namespace {

struct MoveProbeGuard {
    ~MoveProbeGuard() {
        TestHooks::reset_categorization_move_probe();
    }
};

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
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
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

#ifndef _WIN32
TEST_CASE("CategorizationDialog supports sorting by columns") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
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
#endif

#ifndef _WIN32
TEST_CASE("CategorizationDialog undo restores moved files") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
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

TEST_CASE("CategorizationDialog undo allows renaming again") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
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

TEST_CASE("CategorizationDialog rename-only updates cached filename") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
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
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
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
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
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
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
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
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
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
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
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
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
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

TEST_CASE("CategorizationDialog avoids existing picture filename collisions") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
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
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
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

TEST_CASE("CategorizationDialog rename-only preserves cached categories when renaming") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
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
#endif

#endif // !_WIN32
