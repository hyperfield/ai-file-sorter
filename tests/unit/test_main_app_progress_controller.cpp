#include <catch2/catch_test_macros.hpp>

#include "MainAppProgressController.hpp"

namespace {

void run_immediately(std::function<void()> func)
{
    if (func) {
        func();
    }
}

} // namespace

TEST_CASE("MainAppProgressController hides verbose vision diagnostics outside diagnostic modes")
{
    MainAppProgressController controller(run_immediately, run_immediately);

    CHECK_FALSE(controller.should_show_message_in_dialog(
        "[VISION] Runtime: backend=Gemma 3 4B | text=gpu | mmproj=gpu | batch_size=128"));
    CHECK_FALSE(controller.should_show_message_in_dialog(
        "[VISION] Timing sample.jpg: load 16 ms | describe 2.45 s | filename 534 ms"));
    CHECK(controller.should_show_message_in_dialog("[VISION] Using cached suggestion for sample.jpg"));
    CHECK(controller.should_show_message_in_dialog("[DOC] Analyzing invoice.pdf"));
}

TEST_CASE("MainAppProgressController shows verbose vision diagnostics when enabled")
{
    MainAppProgressController controller(run_immediately, run_immediately);
    controller.set_show_vision_diagnostics(true);

    CHECK(controller.should_show_message_in_dialog(
        "[VISION] Runtime: backend=Gemma 3 4B | text=gpu | mmproj=gpu | batch_size=128"));
    CHECK(controller.should_show_message_in_dialog(
        "[VISION] Timing sample.jpg: load 16 ms | describe 2.45 s | filename 534 ms"));
}
