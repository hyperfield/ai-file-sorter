#include <QApplication>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <sstream>
#include <string>
#include <utility>

#include "StoragePluginConformanceHarness.hpp"
#include "TestHelpers.hpp"

namespace {

std::filesystem::path storage_plugin_stub_path() {
#ifdef AIFS_STORAGE_PLUGIN_STUB_NAME
    return std::filesystem::path(QApplication::applicationDirPath().toStdString()) / AIFS_STORAGE_PLUGIN_STUB_NAME;
#else
    return {};
#endif
}

std::string join_failures(const std::vector<std::string>& failures) {
    std::ostringstream stream;
    for (const auto& failure : failures) {
        stream << failure << '\n';
    }
    return stream.str();
}

}  // namespace

TEST_CASE("Storage plugin conformance harness validates the mock connector protocol") {
    QtAppContext qt;
    TempDir fixture_parent;

    StoragePluginConformanceOptions options;
    options.connector_executable = storage_plugin_stub_path();
    options.plugin_id = "mockcloud_storage_support";
    options.provider_id = "mockcloud";
    options.fixture_parent = fixture_parent.path();

    StoragePluginConformanceHarness harness(std::move(options));
    const StoragePluginConformanceResult result = harness.run();

    INFO(join_failures(result.failures));
    CHECK(result.passed);
}
