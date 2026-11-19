#include <QCoreApplication>
#include <iostream>
#include <optional>
#include <string>
#include <utility>

#include "StoragePluginConformanceHarness.hpp"

namespace {

void print_usage(const char* program) {
    std::cerr << "Usage: " << program << " --connector <path> --provider-id <id> [options]\n"
              << "\n"
              << "Options:\n"
              << "  --plugin-id <id>          Plugin id to place in fixture requests.\n"
              << "  --fixture-parent <path>   Parent directory where the fixture tree is created.\n"
              << "  --keep-fixture            Leave generated fixture files on disk.\n"
              << "  --allow-detect-miss       Validate detect response shape without requiring matched=true.\n"
              << "  --timeout-ms <ms>         Per-request timeout. Default: 5000.\n";
}

std::optional<std::string> next_value(int argc, char* argv[], int* index) {
    if (!index || *index + 1 >= argc) {
        return std::nullopt;
    }
    ++(*index);
    return std::string(argv[*index]);
}

bool parse_args(int argc, char* argv[], StoragePluginConformanceOptions* options) {
    if (!options) {
        return false;
    }

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            return false;
        }
        if (arg == "--connector") {
            const auto value = next_value(argc, argv, &i);
            if (!value) {
                return false;
            }
            options->connector_executable = *value;
        } else if (arg == "--provider-id") {
            const auto value = next_value(argc, argv, &i);
            if (!value) {
                return false;
            }
            options->provider_id = *value;
        } else if (arg == "--plugin-id") {
            const auto value = next_value(argc, argv, &i);
            if (!value) {
                return false;
            }
            options->plugin_id = *value;
        } else if (arg == "--fixture-parent") {
            const auto value = next_value(argc, argv, &i);
            if (!value) {
                return false;
            }
            options->fixture_parent = *value;
        } else if (arg == "--keep-fixture") {
            options->keep_fixture = true;
        } else if (arg == "--allow-detect-miss") {
            options->require_detect_match = false;
        } else if (arg == "--timeout-ms") {
            const auto value = next_value(argc, argv, &i);
            if (!value) {
                return false;
            }
            try {
                options->timeout_ms = std::stoi(*value);
            } catch (...) {
                return false;
            }
        } else {
            return false;
        }
    }

    return !options->connector_executable.empty() && !options->provider_id.empty();
}

}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    StoragePluginConformanceOptions options;
    if (!parse_args(argc, argv, &options)) {
        print_usage(argv[0]);
        return 2;
    }

    StoragePluginConformanceHarness harness(std::move(options));
    const StoragePluginConformanceResult result = harness.run();
    if (result.passed) {
        std::cout << "Storage plugin conformance passed for fixture root: " << result.fixture_root.string() << "\n";
        return 0;
    }

    std::cerr << "Storage plugin conformance failed for fixture root: " << result.fixture_root.string() << "\n";
    for (const auto& failure : result.failures) {
        std::cerr << "- " << failure << "\n";
    }
    return 1;
}
