#include <catch2/catch_test_macros.hpp>

#include "MainApp.hpp"
#include "MainAppTestAccess.hpp"
#include "Settings.hpp"
#include "SuitabilityBenchmarkDialog.hpp"
#include "TestHelpers.hpp"
#include "TestHooks.hpp"

#include <QApplication>
#include <QTextEdit>
#include <string>

namespace {

class BenchmarkSignatureProbeGuard {
public:
    ~BenchmarkSignatureProbeGuard()
    {
        TestHooks::reset_benchmark_probe_signature_probe();
    }

    BenchmarkSignatureProbeGuard(const BenchmarkSignatureProbeGuard&) = delete;
    BenchmarkSignatureProbeGuard& operator=(const BenchmarkSignatureProbeGuard&) = delete;

    BenchmarkSignatureProbeGuard()
    {
        TestHooks::reset_benchmark_probe_signature_probe();
    }
};

} // namespace

TEST_CASE("Suppressed suitability benchmark startup skips probe signature computation")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
#ifdef _WIN32
    EnvVarGuard appdata_guard("APPDATA", temp.path().string());
#endif
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    BenchmarkSignatureProbeGuard probe_guard;

    Settings settings;
    settings.set_suitability_benchmark_completed(true);
    settings.set_suitability_benchmark_suppressed(true);
    settings.set_benchmark_probe_signature("old-signature");
    settings.set_benchmark_probe_schema_version(Settings::kBenchmarkProbeSchemaVersion);
    REQUIRE(settings.save());

    int signature_calls = 0;
    TestHooks::set_benchmark_probe_signature_probe([&signature_calls]() {
        ++signature_calls;
        return std::string("current-signature");
    });

    MainApp window(settings, /*development_mode=*/false);
    MainAppTestAccess::set_visual_llm_available_probe(window, []() { return true; });

    MainAppTestAccess::maybe_show_suitability_benchmark(window);
    QApplication::processEvents();

    CHECK(signature_calls == 0);
    CHECK_FALSE(MainAppTestAccess::has_suitability_benchmark_dialog(window));
}

TEST_CASE("Current suitability benchmark startup checks probe signature and stays hidden")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
#ifdef _WIN32
    EnvVarGuard appdata_guard("APPDATA", temp.path().string());
#endif
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    BenchmarkSignatureProbeGuard probe_guard;

    Settings settings;
    settings.set_suitability_benchmark_completed(true);
    settings.set_suitability_benchmark_suppressed(false);
    settings.set_benchmark_probe_signature("current-signature");
    settings.set_benchmark_probe_schema_version(Settings::kBenchmarkProbeSchemaVersion);
    REQUIRE(settings.save());

    int signature_calls = 0;
    TestHooks::set_benchmark_probe_signature_probe([&signature_calls]() {
        ++signature_calls;
        return std::string("current-signature");
    });

    MainApp window(settings, /*development_mode=*/false);
    MainAppTestAccess::set_visual_llm_available_probe(window, []() { return true; });

    MainAppTestAccess::maybe_show_suitability_benchmark(window);
    QApplication::processEvents();

    CHECK(signature_calls == 1);
    CHECK_FALSE(MainAppTestAccess::has_suitability_benchmark_dialog(window));
}

TEST_CASE("Stale suitability benchmark auto-shows when unsuppressed")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
#ifdef _WIN32
    EnvVarGuard appdata_guard("APPDATA", temp.path().string());
#endif
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    BenchmarkSignatureProbeGuard probe_guard;

    Settings settings;
    settings.set_suitability_benchmark_completed(true);
    settings.set_suitability_benchmark_suppressed(false);
    settings.set_benchmark_last_run("2026-06-08 12:00:00");
    settings.set_benchmark_last_report("Backend: CPU\nCUDA: unavailable");
    settings.set_benchmark_probe_signature("old-signature");
    settings.set_benchmark_probe_schema_version(Settings::kBenchmarkProbeSchemaVersion);
    REQUIRE(settings.save());

    TestHooks::set_benchmark_probe_signature_probe([]() {
        return std::string("current-signature");
    });

    MainApp window(settings, /*development_mode=*/false);
    MainAppTestAccess::set_visual_llm_available_probe(window, []() { return true; });

    MainAppTestAccess::maybe_show_suitability_benchmark(window);
    QApplication::processEvents();
    QApplication::processEvents();

    CHECK(MainAppTestAccess::has_suitability_benchmark_dialog(window));
}

TEST_CASE("Stale suitability benchmark previous results include stale warning")
{
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", "offscreen");
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
#ifdef _WIN32
    EnvVarGuard appdata_guard("APPDATA", temp.path().string());
#endif
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    BenchmarkSignatureProbeGuard probe_guard;

    Settings settings;
    settings.set_suitability_benchmark_completed(true);
    settings.set_benchmark_last_run("2026-06-08 12:00:00");
    settings.set_benchmark_last_report("Backend: CPU\nCUDA: unavailable");
    settings.set_benchmark_probe_signature("old-signature");
    settings.set_benchmark_probe_schema_version(Settings::kBenchmarkProbeSchemaVersion);
    REQUIRE(settings.save());

    TestHooks::set_benchmark_probe_signature_probe([]() {
        return std::string("current-signature");
    });

    SuitabilityBenchmarkDialog dialog(settings);
    auto* output = dialog.findChild<QTextEdit*>();
    REQUIRE(output != nullptr);

    const QString text = output->toPlainText();
    const qsizetype last_run_index =
        text.indexOf(QStringLiteral("Last run: 2026-06-08 12:00:00"));
    const qsizetype stale_warning_index = text.indexOf(QStringLiteral(
        "Previous result may be stale because the CUDA/backend probe changed."));
    const qsizetype previous_results_index =
        text.indexOf(QStringLiteral("Previous results:"));
    const qsizetype backend_line_index = text.indexOf(QStringLiteral("Backend: CPU"));

    CHECK(text.contains(QStringLiteral("Last run: 2026-06-08 12:00:00")));
    CHECK(stale_warning_index != -1);
    CHECK(text.contains(QStringLiteral("Previous results:")));
    CHECK(text.contains(QStringLiteral("Backend: CPU")));
    CHECK(text.contains(QStringLiteral("CUDA: unavailable")));
    CHECK(last_run_index != -1);
    CHECK(previous_results_index != -1);
    CHECK(backend_line_index != -1);
    CHECK(last_run_index < stale_warning_index);
    CHECK(stale_warning_index < previous_results_index);
    CHECK(previous_results_index < backend_line_index);
}
