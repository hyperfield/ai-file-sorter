#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace WindowsCudaProbe {

struct ProbeResult {
    bool driver_present{false};
    bool driver_initialized{false};
    int driver_version{0};
    int device_count{0};
    bool runtime_present{false};
    bool runtime_usable{false};
    bool backend_loadable{false};
    int runtime_version_token{0};
    std::filesystem::path runtime_library_path;
    std::string failure_reason;
};

ProbeResult probe(const std::optional<std::filesystem::path>& ggml_directory = std::nullopt);
std::optional<std::filesystem::path> best_runtime_library_path();
int installed_runtime_version_token();
std::string best_runtime_library_name();

} // namespace WindowsCudaProbe
