#include "StorageUndoCleanup.hpp"

#include "StorageProvider.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace {

std::string directory_key(const std::filesystem::path& path)
{
    std::string key = Utils::path_to_utf8(path.lexically_normal());
#ifdef _WIN32
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
#endif
    return key;
}

bool is_root_or_empty(const std::filesystem::path& path)
{
    return path.empty() || path == path.root_path();
}

bool key_is_same_or_parent(std::string parent, const std::string& child)
{
    if (parent.empty()) {
        return false;
    }
    if (parent == child) {
        return true;
    }
    const char tail = parent.back();
    if (tail != '/' && tail != '\\') {
        parent.push_back(static_cast<char>(std::filesystem::path::preferred_separator));
    }
    return child.rfind(parent, 0) == 0;
}

} // namespace

namespace StorageUndoCleanup {

std::vector<std::string> missing_directories_for_target(const IStorageProvider& provider,
                                                        const std::string& target_directory)
{
    std::vector<std::filesystem::path> missing;
    std::filesystem::path current = Utils::utf8_to_path(target_directory).lexically_normal();

    while (!is_root_or_empty(current)) {
        const std::string current_text = Utils::path_to_utf8(current);
        if (provider.path_exists(current_text)) {
            break;
        }

        missing.push_back(current);
        const auto parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }

    std::reverse(missing.begin(), missing.end());

    std::vector<std::string> result;
    result.reserve(missing.size());
    for (const auto& path : missing) {
        result.push_back(Utils::path_to_utf8(path));
    }
    return result;
}

std::vector<std::string> recorded_directories_for_target(
    const std::vector<std::string>& created_directories,
    const std::string& target_directory)
{
    const auto target_path = Utils::utf8_to_path(target_directory).lexically_normal();
    const std::string target_key = directory_key(target_path);

    std::vector<std::filesystem::path> matching;
    matching.reserve(created_directories.size());
    std::unordered_set<std::string> seen;
    for (const auto& directory : created_directories) {
        auto path = Utils::utf8_to_path(directory).lexically_normal();
        if (is_root_or_empty(path)) {
            continue;
        }

        const auto key = directory_key(path);
        if (!key_is_same_or_parent(key, target_key)) {
            continue;
        }
        if (seen.insert(key).second) {
            matching.push_back(std::move(path));
        }
    }

    std::sort(matching.begin(), matching.end(), [](const auto& left, const auto& right) {
        return left.native().size() < right.native().size();
    });

    std::vector<std::string> result;
    result.reserve(matching.size());
    for (const auto& path : matching) {
        result.push_back(Utils::path_to_utf8(path));
    }
    return result;
}

std::size_t remove_empty_created_directories(const std::vector<std::string>& directories)
{
    std::vector<std::filesystem::path> ordered;
    ordered.reserve(directories.size());
    std::unordered_set<std::string> seen;

    for (const auto& directory : directories) {
        if (directory.empty()) {
            continue;
        }
        auto path = Utils::utf8_to_path(directory).lexically_normal();
        if (is_root_or_empty(path)) {
            continue;
        }
        const auto key = directory_key(path);
        if (seen.insert(key).second) {
            ordered.push_back(std::move(path));
        }
    }

    std::sort(ordered.begin(), ordered.end(), [](const auto& left, const auto& right) {
        return left.native().size() > right.native().size();
    });

    std::size_t removed = 0;
    for (const auto& directory : ordered) {
        std::error_code ec;
        if (!std::filesystem::exists(directory, ec) || ec) {
            continue;
        }
        if (!std::filesystem::is_directory(directory, ec) || ec) {
            continue;
        }
        if (!std::filesystem::is_empty(directory, ec) || ec) {
            continue;
        }
        std::filesystem::remove(directory, ec);
        if (!ec) {
            ++removed;
        }
    }
    return removed;
}

std::size_t remove_empty_parent_directories(const std::string& moved_from)
{
    std::error_code ec;
    auto parent = Utils::utf8_to_path(moved_from).parent_path();
    std::size_t removed = 0;
    while (!is_root_or_empty(parent)) {
        if (!std::filesystem::exists(parent, ec) || ec) {
            break;
        }
        if (std::filesystem::is_directory(parent, ec) &&
            std::filesystem::is_empty(parent, ec) && !ec) {
            std::filesystem::remove(parent, ec);
            if (!ec) {
                ++removed;
            }
            parent = parent.parent_path();
            continue;
        }
        break;
    }
    return removed;
}

} // namespace StorageUndoCleanup
