#include "GgmlRuntimePaths.hpp"

#include <algorithm>
#include <array>

namespace GgmlRuntimePaths {

namespace {

bool ends_with(const std::string& value, const std::string& suffix) {
    if (suffix.size() > value.size()) {
        return false;
    }
    return std::equal(suffix.rbegin(), suffix.rend(), value.rbegin());
}

} // namespace

bool has_payload(const std::filesystem::path& dir) {
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) {
        return false;
    }

    for (const auto& entry : std::filesystem::directory_iterator(
             dir,
             std::filesystem::directory_options::skip_permission_denied,
             ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        const std::string filename = entry.path().filename().string();
        if (filename.rfind("libggml-", 0) != 0) {
            continue;
        }
        if (ends_with(filename, ".so") || ends_with(filename, ".dylib")) {
            return true;
        }
    }

    return false;
}

std::vector<std::filesystem::path> macos_candidate_dirs(
    const std::filesystem::path& exe_path,
    std::string_view ggml_subdir) {
    if (exe_path.empty()) {
        return {};
    }

    const std::filesystem::path exe_dir = exe_path.parent_path();
    const std::filesystem::path subdir(ggml_subdir);

    return {
        exe_dir / "../lib" / "precompiled-m1",
        exe_dir / "../lib" / "precompiled-m2",
        exe_dir / "../lib" / "precompiled-intel",
        exe_dir / "../lib" / subdir,
        exe_dir / "../../lib" / subdir,
        exe_dir / "../lib" / "aifilesorter",
        exe_dir / "../../lib" / "aifilesorter",
        exe_dir / "../lib",
        exe_dir / "../../lib",
    };
}

std::optional<std::filesystem::path> resolve_macos_backend_dir(
    const std::optional<std::filesystem::path>& current_dir,
    const std::filesystem::path& exe_path,
    std::string_view ggml_subdir) {
    if (current_dir && has_payload(*current_dir)) {
        return current_dir->lexically_normal();
    }

    for (const auto& candidate : macos_candidate_dirs(exe_path, ggml_subdir)) {
        if (has_payload(candidate)) {
            return candidate.lexically_normal();
        }
    }

    return std::nullopt;
}

} // namespace GgmlRuntimePaths
