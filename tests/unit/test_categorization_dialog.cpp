#include <catch2/catch_test_macros.hpp>
#include "CategorizationDialog.hpp"
#include "TestHooks.hpp"
#include "TestHelpers.hpp"
#include <filesystem>
#include <fstream>

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
        CategorizationDialog dialog(nullptr, initial_state);
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

    CategorizationDialog dialog(nullptr, true);
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
