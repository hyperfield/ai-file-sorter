#include <catch2/catch_test_macros.hpp>

#include "Settings.hpp"
#include "SuitabilityBenchmarkDialog.hpp"
#include "SuitabilityBenchmarkDialogTestAccess.hpp"
#include "TestHelpers.hpp"
#include "TestHooks.hpp"

#include <string>

namespace {

class BenchmarkSignatureProbeGuard {
public:
    BenchmarkSignatureProbeGuard()
    {
        TestHooks::reset_benchmark_probe_signature_probe();
    }

    ~BenchmarkSignatureProbeGuard()
    {
        TestHooks::reset_benchmark_probe_signature_probe();
    }

    BenchmarkSignatureProbeGuard(const BenchmarkSignatureProbeGuard&) = delete;
    BenchmarkSignatureProbeGuard& operator=(const BenchmarkSignatureProbeGuard&) = delete;
};

} // namespace

TEST_CASE("Suitability benchmark finish persists current probe metadata") {
    QtAppContext qt;
    TempDir config_dir;
    EnvVarGuard home_guard("HOME", config_dir.path().string());
#ifdef _WIN32
    EnvVarGuard appdata_guard("APPDATA", config_dir.path().string());
#endif
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    BenchmarkSignatureProbeGuard probe_guard;

    TestHooks::set_benchmark_probe_signature_probe([]() {
        return std::string("dialog-current-signature");
    });

    Settings settings;
    REQUIRE_FALSE(settings.load());

    SuitabilityBenchmarkDialog dialog(settings);
    SuitabilityBenchmarkDialogTestAccess::finish_benchmark(dialog);

    Settings reloaded;
    REQUIRE(reloaded.load());

    const std::string signature = "dialog-current-signature";
    REQUIRE(reloaded.get_suitability_benchmark_completed());
    REQUIRE(reloaded.get_benchmark_probe_schema_version() ==
            Settings::kBenchmarkProbeSchemaVersion);
    REQUIRE(reloaded.get_benchmark_probe_signature() == signature);
    REQUIRE(reloaded.is_suitability_benchmark_current(signature));
}
