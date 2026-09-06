#include <catch2/catch_test_macros.hpp>

#include "MainApp.hpp"
#include "MainAppTestAccess.hpp"
#include "Settings.hpp"
#include "TestHelpers.hpp"

#include <string>

TEST_CASE("Visual CPU fallback detection recognizes retryable GPU failures") {
    CHECK(MainAppTestAccess::should_offer_visual_cpu_fallback(
        "Failed to create llama_context (try AI_FILE_SORTER_VISUAL_USE_GPU=0 to force CPU)"));
    CHECK(MainAppTestAccess::should_offer_visual_cpu_fallback("mtmd_helper_eval_chunks failed"));
    CHECK(MainAppTestAccess::should_offer_visual_cpu_fallback("VK_ERROR_OUT_OF_DEVICE_MEMORY"));
    CHECK(MainAppTestAccess::should_offer_visual_cpu_fallback(
        "vk::Device::allocateMemory: ErrorOutOfDeviceMemory"));
    CHECK(MainAppTestAccess::should_offer_visual_cpu_fallback(
        "vk::Device::allocateMemory: ErrorOutOfHostMemory"));
    CHECK(MainAppTestAccess::should_offer_visual_cpu_fallback("CUDA error out of memory"));
    CHECK(MainAppTestAccess::should_offer_visual_cpu_fallback(
        "Visual GPU preflight crashed (exit code 0xC0000409)"));
}

TEST_CASE("Visual CPU fallback detection ignores non-retryable startup failures") {
    CHECK_FALSE(MainAppTestAccess::should_offer_visual_cpu_fallback(
        "Failed to load multimodal projector file at C:/models/mmproj.gguf"));
    CHECK_FALSE(MainAppTestAccess::should_offer_visual_cpu_fallback(
        "Failed to load visual text model at C:/models/model.gguf"));
    CHECK_FALSE(MainAppTestAccess::should_offer_visual_cpu_fallback(
        "The provided multimodal projector does not expose vision capabilities"));
}

TEST_CASE("Visual CPU fallback decline requests analysis cancellation")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    Settings settings;
    REQUIRE(settings.save());
    MainApp window(settings, /*development_mode=*/false);

    int prompt_count = 0;
    MainAppTestAccess::set_visual_cpu_fallback_prompt_override(window, [&prompt_count]() {
        ++prompt_count;
        return false;
    });

    CHECK_FALSE(MainAppTestAccess::prompt_visual_cpu_fallback(window, "VK_ERROR_OUT_OF_DEVICE_MEMORY"));
    CHECK(MainAppTestAccess::stop_analysis_requested(window));
    CHECK(prompt_count == 1);

    CHECK_FALSE(MainAppTestAccess::prompt_visual_cpu_fallback(window, "later retry"));
    CHECK(prompt_count == 1);
}

TEST_CASE("Visual CPU fallback acceptance keeps analysis running")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    Settings settings;
    REQUIRE(settings.save());
    MainApp window(settings, /*development_mode=*/false);

    MainAppTestAccess::set_visual_cpu_fallback_prompt_override(window, []() { return true; });

    CHECK(MainAppTestAccess::prompt_visual_cpu_fallback(window, "VK_ERROR_OUT_OF_DEVICE_MEMORY"));
    CHECK_FALSE(MainAppTestAccess::stop_analysis_requested(window));
}

TEST_CASE("Continue-without-visual-analysis decline requests analysis cancellation")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    Settings settings;
    REQUIRE(settings.save());
    MainApp window(settings, /*development_mode=*/false);

    int prompt_count = 0;
    MainAppTestAccess::set_continue_without_visual_analysis_prompt_override(
        window,
        [&prompt_count]() {
            ++prompt_count;
            return false;
        });

    CHECK_FALSE(MainAppTestAccess::prompt_continue_without_visual_analysis(
        window,
        "Visual model artifact is invalid or incomplete (expected GGUF header): model.gguf"));
    CHECK(MainAppTestAccess::stop_analysis_requested(window));
    CHECK(prompt_count == 1);

    CHECK_FALSE(MainAppTestAccess::prompt_continue_without_visual_analysis(window, "later retry"));
    CHECK(prompt_count == 1);
}

TEST_CASE("Continue-without-visual-analysis acceptance keeps analysis running")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    Settings settings;
    REQUIRE(settings.save());
    MainApp window(settings, /*development_mode=*/false);

    MainAppTestAccess::set_continue_without_visual_analysis_prompt_override(
        window,
        []() { return true; });

    CHECK(MainAppTestAccess::prompt_continue_without_visual_analysis(
        window,
        "Visual model artifact is invalid or incomplete (expected GGUF header): model.gguf"));
    CHECK_FALSE(MainAppTestAccess::stop_analysis_requested(window));
}

TEST_CASE("Vision diagnostics are only shown in the progress dialog for development or test mode")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());

    Settings settings;
    REQUIRE(settings.save());

    const std::string runtime_message =
        "[VISION] Runtime: backend=Gemma 3 4B | text=gpu | mmproj=gpu | batch_size=128";
    const std::string timing_message =
        "[VISION] Timing lion-805084_1920-c62a5582169c4bae82553d9a21c1a0bb.jpg: "
        "load 16.4 ms | describe 2.45 s total (tokenize 16.4 ms, eval 1.85 s, gen 582 ms) | "
        "filename 534 ms total (tokenize 1.0 ms, eval 375 ms, gen 158 ms) | total 3.00 s";
    const std::string decoding_message = "[VISION] Decoding image batch 1/2 (50.00%)";
    const std::string ordinary_message = "[VISION] Using cached suggestion for lion-805084_1920.jpg";

    SECTION("normal mode hides vision diagnostics") {
        MainApp window(settings, /*development_mode=*/false, /*test_mode=*/false);
        CHECK_FALSE(MainAppTestAccess::should_show_progress_message_in_dialog(window, runtime_message));
        CHECK_FALSE(MainAppTestAccess::should_show_progress_message_in_dialog(window, timing_message));
        CHECK_FALSE(MainAppTestAccess::should_show_progress_message_in_dialog(window, decoding_message));
        CHECK(MainAppTestAccess::should_show_progress_message_in_dialog(window, ordinary_message));
    }

    SECTION("development mode shows vision diagnostics") {
        MainApp window(settings, /*development_mode=*/true, /*test_mode=*/false);
        CHECK(MainAppTestAccess::should_show_progress_message_in_dialog(window, runtime_message));
        CHECK(MainAppTestAccess::should_show_progress_message_in_dialog(window, timing_message));
        CHECK(MainAppTestAccess::should_show_progress_message_in_dialog(window, decoding_message));
    }

    SECTION("test mode shows vision diagnostics") {
        MainApp window(settings, /*development_mode=*/false, /*test_mode=*/true);
        CHECK(MainAppTestAccess::should_show_progress_message_in_dialog(window, runtime_message));
        CHECK(MainAppTestAccess::should_show_progress_message_in_dialog(window, timing_message));
        CHECK(MainAppTestAccess::should_show_progress_message_in_dialog(window, decoding_message));
    }
}
