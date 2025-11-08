#include <catch2/catch_test_macros.hpp>
#include "CategorizationDialog.hpp"
#include "TestHooks.hpp"
#include "TestHelpers.hpp"

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
