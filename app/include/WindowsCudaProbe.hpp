#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace WindowsCudaProbe {

enum class RuntimeSource {
    None,
    Packaged,
    ToolkitHint,
    Path,
};

struct ProbeOptions {
    std::optional<std::filesystem::path> ggml_directory;
    std::vector<std::filesystem::path> preferred_runtime_directories;
    bool include_system_directories{true};
};

struct ProbeResult {
    bool driver_present{false};
    bool driver_initialized{false};
    int driver_version{0};
    int device_count{0};
    bool runtime_present{false};
    bool runtime_usable{false};
    bool backend_loadable{false};
    int runtime_version_token{0};
    RuntimeSource runtime_source{RuntimeSource::None};
    std::filesystem::path runtime_library_path;
    std::string failure_reason;
};

ProbeResult probe(const std::optional<std::filesystem::path>& ggml_directory = std::nullopt);
ProbeResult probe(const ProbeOptions& options);
std::optional<std::filesystem::path> best_runtime_library_path();
std::optional<std::filesystem::path> best_runtime_library_path(const ProbeOptions& options);
int installed_runtime_version_token();
int installed_runtime_version_token(const ProbeOptions& options);
std::string best_runtime_library_name();
std::string best_runtime_library_name(const ProbeOptions& options);
std::string runtime_source_name(RuntimeSource source);

#ifdef AI_FILE_SORTER_TEST_BUILD
namespace TestAccess {
int runtime_version_rank(std::string_view file_name);
std::vector<std::filesystem::path> rank_runtime_candidates(
    const std::vector<std::filesystem::path>& runtime_paths);
std::vector<std::filesystem::path> rank_runtime_candidates_with_sources(
    const std::vector<std::filesystem::path>& packaged_runtime_paths,
    const std::vector<std::filesystem::path>& toolkit_hint_runtime_paths,
    const std::vector<std::filesystem::path>& path_runtime_paths);
std::vector<std::filesystem::path> candidate_runtime_directories(const ProbeOptions& options);
} // namespace TestAccess
#endif

} // namespace WindowsCudaProbe
