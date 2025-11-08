#include <catch2/catch_test_macros.hpp>
#include "MainApp.hpp"
#include "MainAppTestAccess.hpp"
#include "TestHelpers.hpp"
#include "Settings.hpp"
#include <filesystem>

namespace {

struct TestEnvironment {
    TempDir home_dir;
    EnvVarGuard home_guard;
    Settings settings;
    bool prompt_state{false};

    TestEnvironment()
        : home_dir(),
          home_guard("HOME", home_dir.path().string()),
          settings() {
        std::filesystem::create_directories(home_dir.path() / ".config" / "AIFileSorter");
        settings.load();
    }
};

} // namespace

static void run_support_prompt_case(MainAppTestAccess::SimulatedSupportResult response,
                                    int expected_increment) {
    TestEnvironment env;
    std::vector<int> totals;

    auto callback = [&](int total) {
        totals.push_back(total);
        return response;
    };

    REQUIRE(env.settings.get_total_categorized_files() == 0);
    REQUIRE(env.settings.get_next_support_prompt_threshold() == 100);

    MainAppTestAccess::simulate_support_prompt(env.settings, env.prompt_state, 50, callback);
    CHECK(env.settings.get_total_categorized_files() == 50);
    CHECK(totals.empty());

    MainAppTestAccess::simulate_support_prompt(env.settings, env.prompt_state, 50, callback);
    CHECK(env.settings.get_total_categorized_files() == 100);
    REQUIRE(totals.size() == 1);
    CHECK(totals.front() == 100);
    CHECK(env.settings.get_next_support_prompt_threshold() == 100 + expected_increment);

    MainAppTestAccess::simulate_support_prompt(env.settings, env.prompt_state, expected_increment - 1, callback);
    CHECK(totals.size() == 1);

    MainAppTestAccess::simulate_support_prompt(env.settings, env.prompt_state, 1, callback);
    CHECK(env.settings.get_total_categorized_files() == 100 + expected_increment);
    REQUIRE(totals.size() == 2);
    CHECK(totals.back() == 100 + expected_increment);
    CHECK(env.settings.get_next_support_prompt_threshold() == 100 + expected_increment * 2);
}

TEST_CASE("Support prompt thresholds advance based on response") {
    SECTION("Not sure response prompts every 100 files") {
        run_support_prompt_case(MainAppTestAccess::SimulatedSupportResult::NotSure, 100);
    }

    SECTION("Cannot donate defers prompt by 500 files") {
        run_support_prompt_case(MainAppTestAccess::SimulatedSupportResult::CannotDonate, 500);
    }

    SECTION("Support response also defers by 500 files") {
        run_support_prompt_case(MainAppTestAccess::SimulatedSupportResult::Support, 500);
    }
}
