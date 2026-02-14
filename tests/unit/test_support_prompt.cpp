#include <catch2/catch_test_macros.hpp>
#include "MainApp.hpp"
#include "MainAppTestAccess.hpp"
#include "TestHelpers.hpp"
#include "Settings.hpp"
#include <algorithm>
#include <filesystem>

namespace {

struct TestEnvironment {
    TempDir home_dir;
    EnvVarGuard home_guard;
    EnvVarGuard config_guard;
    Settings settings;
    bool prompt_state{false};

    TestEnvironment()
        : home_dir(),
          home_guard("HOME", home_dir.path().string()),
          config_guard("AI_FILE_SORTER_CONFIG_DIR", (home_dir.path() / ".config").string()),
          settings() {
        std::filesystem::create_directories(home_dir.path() / ".config" / "AIFileSorter");
        settings.load();
    }
};

} // namespace

static void run_support_prompt_case(MainAppTestAccess::SimulatedSupportResult response) {
    TestEnvironment env;
    std::vector<int> totals;

    auto callback = [&](int total) {
        totals.push_back(total);
        return response;
    };

    REQUIRE(env.settings.get_total_categorized_files() == 0);
    const int base_threshold = env.settings.get_next_support_prompt_threshold();
    REQUIRE(base_threshold > 0);

    const int first_step = std::max(1, base_threshold / 2);
    const int second_step = base_threshold - first_step;

    MainAppTestAccess::simulate_support_prompt(env.settings, env.prompt_state, first_step, callback);
    CHECK(env.settings.get_total_categorized_files() == first_step);
    CHECK(totals.empty());

    MainAppTestAccess::simulate_support_prompt(env.settings, env.prompt_state, second_step, callback);
    CHECK(env.settings.get_total_categorized_files() == base_threshold);
    REQUIRE(totals.size() == 1);
    CHECK(totals.front() == base_threshold);
    const int next_threshold = env.settings.get_next_support_prompt_threshold();
    CHECK(next_threshold > base_threshold);
    const int increment = next_threshold - base_threshold;
    CHECK(increment > 0);

    if (increment > 1) {
        MainAppTestAccess::simulate_support_prompt(env.settings, env.prompt_state, increment - 1, callback);
        CHECK(totals.size() == 1);

        MainAppTestAccess::simulate_support_prompt(env.settings, env.prompt_state, 1, callback);
    } else {
        MainAppTestAccess::simulate_support_prompt(env.settings, env.prompt_state, 1, callback);
    }
    CHECK(env.settings.get_total_categorized_files() == base_threshold + increment);
    REQUIRE(totals.size() == 2);
    CHECK(totals.back() == base_threshold + increment);
    CHECK(env.settings.get_next_support_prompt_threshold() > next_threshold);
}

TEST_CASE("Support prompt thresholds advance based on response") {
    SECTION("Not sure response schedules another prompt") {
        run_support_prompt_case(MainAppTestAccess::SimulatedSupportResult::NotSure);
    }

    SECTION("Cannot donate response defers prompt") {
        run_support_prompt_case(MainAppTestAccess::SimulatedSupportResult::CannotDonate);
    }

    SECTION("Support response defers prompt") {
        run_support_prompt_case(MainAppTestAccess::SimulatedSupportResult::Support);
    }
}

TEST_CASE("Zero categorized increments do not change totals or trigger prompts") {
    TestEnvironment env;
    bool callback_invoked = false;
    const int base_threshold = env.settings.get_next_support_prompt_threshold();

    MainAppTestAccess::simulate_support_prompt(
        env.settings,
        env.prompt_state,
        0,
        [&](int) {
            callback_invoked = true;
            return MainAppTestAccess::SimulatedSupportResult::NotSure;
        });

    CHECK(env.settings.get_total_categorized_files() == 0);
    CHECK_FALSE(callback_invoked);
    CHECK(env.settings.get_next_support_prompt_threshold() == base_threshold);
}
