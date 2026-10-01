#pragma once

#include <filesystem>
#include <string>
#include <vector>

/**
 * @brief Options used when running the storage-plugin protocol conformance harness.
 */
struct StoragePluginConformanceOptions {
    /**
     * @brief Candidate storage connector executable to launch for every fixture request.
     */
    std::filesystem::path connector_executable;

    /**
     * @brief Plugin id placed in fixture requests.
     */
    std::string plugin_id{"storage_plugin_conformance"};

    /**
     * @brief Provider id placed in provider-specific fixture requests.
     */
    std::string provider_id{"mockcloud"};

    /**
     * @brief Optional parent directory for the generated fixture tree.
     */
    std::filesystem::path fixture_parent;

    /**
     * @brief Preserve generated fixture files after the harness exits.
     */
    bool keep_fixture{false};

    /**
     * @brief Require the connector to positively detect the generated fixture root.
     */
    bool require_detect_match{true};

    /**
     * @brief Per-request process timeout in milliseconds.
     */
    int timeout_ms{5000};
};

/**
 * @brief Result returned by the storage-plugin protocol conformance harness.
 */
struct StoragePluginConformanceResult {
    /**
     * @brief True when every fixture request received a conforming response.
     */
    bool passed{false};

    /**
     * @brief Human-readable validation failures.
     */
    std::vector<std::string> failures;

    /**
     * @brief Generated fixture root used for the run.
     */
    std::filesystem::path fixture_root;
};

/**
 * @brief Launches external storage connectors and validates them against the v1 JSON protocol.
 */
class StoragePluginConformanceHarness {
   public:
    /**
     * @brief Create a harness with the supplied candidate connector options.
     * @param options Connector path, provider id, fixture location, and validation settings.
     */
    explicit StoragePluginConformanceHarness(StoragePluginConformanceOptions options);

    /**
     * @brief Run the full v1 protocol conformance fixture suite.
     * @return Aggregate pass/fail result and any failures.
     */
    StoragePluginConformanceResult run();

   private:
    StoragePluginConformanceOptions options_;
};
