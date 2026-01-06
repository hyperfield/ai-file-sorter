#include <catch2/catch_test_macros.hpp>
#include "CategorizationDialog.hpp"
#include "DatabaseManager.hpp"
#include "TestHooks.hpp"
#include "TestHelpers.hpp"
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

    const auto old_cache = db.get_categorization_from_db(file_name, FileType::File);
    REQUIRE(old_cache.empty());

    const auto cached = db.get_categorized_files(base.string());
    REQUIRE(cached.size() == 1);
    CHECK(cached.front().file_name == renamed);
    CHECK(cached.front().rename_only);
    CHECK(cached.front().suggested_name == renamed);
}
#endif

#endif // !_WIN32
