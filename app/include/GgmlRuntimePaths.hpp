#pragma once

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace GgmlRuntimePaths {

bool has_payload(const std::filesystem::path& dir);

std::vector<std::filesystem::path> macos_candidate_dirs(
    const std::filesystem::path& exe_path,
    std::string_view ggml_subdir);

std::optional<std::filesystem::path> resolve_macos_backend_dir(
    const std::optional<std::filesystem::path>& current_dir,
    const std::filesystem::path& exe_path,
    std::string_view ggml_subdir);

} // namespace GgmlRuntimePaths
