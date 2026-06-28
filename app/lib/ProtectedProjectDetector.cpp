#include "ProtectedProjectDetector.hpp"

#include <algorithm>
#include <cctype>
#include <system_error>
#include <utility>

namespace {

std::string lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool path_exists(const std::filesystem::path& path)
{
    std::error_code ec;
    return std::filesystem::exists(path, ec) && !ec;
}

bool all_required_paths_exist(const std::filesystem::path& root,
                              const std::vector<std::filesystem::path>& paths)
{
    return std::all_of(paths.begin(), paths.end(), [&root](const std::filesystem::path& marker) {
        return path_exists(root / marker);
    });
}

bool any_path_in_group_exists(const std::filesystem::path& root,
                              const std::vector<std::filesystem::path>& group)
{
    return std::any_of(group.begin(), group.end(), [&root](const std::filesystem::path& marker) {
        return path_exists(root / marker);
    });
}

bool all_any_groups_match(const std::filesystem::path& root,
                          const std::vector<std::vector<std::filesystem::path>>& groups)
{
    return std::all_of(groups.begin(), groups.end(), [&root](const auto& group) {
        return any_path_in_group_exists(root, group);
    });
}

bool has_root_entry_with_suffix(const std::filesystem::path& root,
                                const std::vector<std::string>& suffixes)
{
    if (suffixes.empty()) {
        return true;
    }

    std::error_code ec;
    std::filesystem::directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec) {
        return false;
    }

    const std::filesystem::directory_iterator end;
    for (; it != end; it.increment(ec)) {
        if (ec) {
            return false;
        }

        const std::string name = lower_copy(it->path().filename().string());
        for (const auto& suffix : suffixes) {
            const std::string lowered_suffix = lower_copy(suffix);
            if (!lowered_suffix.empty() && name.ends_with(lowered_suffix)) {
                return true;
            }
        }
    }
    return false;
}

bool rule_has_any_marker(const ProtectedProjectRule& rule)
{
    return !rule.required_paths.empty() ||
           !rule.any_path_groups.empty() ||
           !rule.root_entry_suffixes_any.empty();
}

bool rule_matches(const std::filesystem::path& root, const ProtectedProjectRule& rule)
{
    if (!rule_has_any_marker(rule)) {
        return false;
    }

    return all_required_paths_exist(root, rule.required_paths) &&
           all_any_groups_match(root, rule.any_path_groups) &&
           has_root_entry_with_suffix(root, rule.root_entry_suffixes_any);
}

} // namespace

const std::vector<ProtectedProjectRule>& built_in_protected_project_rules()
{
    static const std::vector<ProtectedProjectRule> rules = {
        {
            "unity",
            "Unity project",
            {"Assets", "ProjectSettings/ProjectVersion.txt"},
            {},
            {},
            ProtectedProjectStrength::Strong,
            "Unity relies on project-relative assets and .meta GUID mappings."
        },
        {
            "unreal",
            "Unreal Engine project",
            {"Config", "Content"},
            {},
            {".uproject"},
            ProtectedProjectStrength::Strong,
            "Unreal projects depend on Content, Config, and .uproject-relative paths."
        },
        {
            "godot",
            "Godot project",
            {"project.godot"},
            {},
            {},
            ProtectedProjectStrength::Strong,
            "Godot projects depend on project.godot and resource-relative paths."
        },
        {
            "git",
            "Git repository",
            {".git"},
            {},
            {},
            ProtectedProjectStrength::Strong,
            "Source repositories depend on stable relative paths tracked by version control."
        },
        {
            "node",
            "Node.js project",
            {"package.json"},
            {{"package-lock.json", "pnpm-lock.yaml", "yarn.lock", "node_modules", "src"}},
            {},
            ProtectedProjectStrength::Strong,
            "JavaScript projects depend on package metadata, imports, and build scripts."
        },
        {
            "python",
            "Python project",
            {"pyproject.toml"},
            {},
            {},
            ProtectedProjectStrength::Strong,
            "Python projects depend on package metadata and source-relative imports."
        },
        {
            "rust",
            "Rust project",
            {"Cargo.toml"},
            {},
            {},
            ProtectedProjectStrength::Strong,
            "Rust projects depend on Cargo metadata and source-relative module paths."
        },
        {
            "go",
            "Go module",
            {"go.mod"},
            {},
            {},
            ProtectedProjectStrength::Strong,
            "Go modules depend on module-root-relative package layout."
        },
        {
            "gradle",
            "Gradle project",
            {},
            {{"settings.gradle", "settings.gradle.kts"}, {"build.gradle", "build.gradle.kts", "gradlew"}},
            {},
            ProtectedProjectStrength::Strong,
            "Gradle projects depend on build files and source-relative layouts."
        },
        {
            "dotnet",
            ".NET project",
            {},
            {},
            {".sln", ".csproj", ".fsproj", ".vbproj"},
            ProtectedProjectStrength::Strong,
            ".NET projects depend on solution and project-relative paths."
        },
        {
            "xcode",
            "Xcode project",
            {},
            {},
            {".xcodeproj", ".xcworkspace"},
            ProtectedProjectStrength::Strong,
            "Xcode projects depend on bundle metadata and project-relative paths."
        },
        {
            "blender",
            "Blender project",
            {},
            {{"assets", "textures", "materials", "renders", "render", "cache", "blendcache"}},
            {".blend"},
            ProtectedProjectStrength::Strong,
            "Blender project folders often depend on sibling asset and cache paths."
        },
        {
            "blender-file",
            "Blender scene folder",
            {},
            {},
            {".blend"},
            ProtectedProjectStrength::Weak,
            "A .blend file alone is a weak signal; no automatic scan skip is applied."
        }
    };
    return rules;
}

ProtectedProjectDetector::ProtectedProjectDetector()
    : rules_(&built_in_protected_project_rules())
{
}

ProtectedProjectDetector::ProtectedProjectDetector(std::vector<ProtectedProjectRule> rules)
    : owned_rules_(std::move(rules)),
      rules_(&owned_rules_)
{
}

std::optional<ProtectedProjectMatch> ProtectedProjectDetector::detect(
    const std::filesystem::path& directory) const
{
    if (!rules_) {
        return std::nullopt;
    }

    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec) || ec) {
        return std::nullopt;
    }

    for (const auto& rule : *rules_) {
        if (!rule_matches(directory, rule)) {
            continue;
        }
        return ProtectedProjectMatch{
            directory,
            rule.id,
            rule.name,
            rule.strength,
            rule.reason
        };
    }
    return std::nullopt;
}

bool ProtectedProjectDetector::should_skip(const ProtectedProjectMatch& match)
{
    return match.strength == ProtectedProjectStrength::Strong;
}
