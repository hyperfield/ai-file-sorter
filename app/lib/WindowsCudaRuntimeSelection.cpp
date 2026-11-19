#include "WindowsCudaRuntimeSelection.hpp"

#include <algorithm>
#include <charconv>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>

namespace WindowsCudaRuntimeSelection {
namespace {
constexpr int kVersionRankBase = 100;
constexpr int kLegacySuffixThreshold = 50;
constexpr int kLegacySuffixDivisor = 10;
constexpr std::size_t kThreeDigitSuffixLength = 3;
constexpr std::string_view kToolkitRootMarker = "/nvidia gpu computing toolkit/cuda/";
constexpr std::string_view kPhysXRootMarker = "/nvidia corporation/physx/common";

struct ParsedNumber {
    int value{0};
    std::size_t consumed{0};
};

std::filesystem::path windows_lexical_path(const std::filesystem::path& path)
{
    auto text = path.generic_u8string();
    std::replace(text.begin(), text.end(), u8'\\', u8'/');
    return std::filesystem::path(text).lexically_normal();
}

std::string path_key(const std::filesystem::path& path)
{
    const auto text = path.generic_u8string();
    std::string key(text.begin(), text.end());
    std::transform(key.begin(), key.end(), key.begin(), [](char ch) {
        return ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch + ('a' - 'A')) : ch;
    });
    return key;
}

std::optional<ParsedNumber> parse_decimal_prefix(std::string_view text)
{
    if (text.empty() || text.front() < '0' || text.front() > '9') {
        return std::nullopt;
    }
    int value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{}) {
        return std::nullopt;
    }
    return ParsedNumber{value, static_cast<std::size_t>(end - text.data())};
}

int normalize_version_rank(int token, std::size_t digits)
{
    if (digits >= kThreeDigitSuffixLength) {
        const int divisor = digits == kThreeDigitSuffixLength ? kLegacySuffixDivisor : kVersionRankBase;
        return (token / divisor) * kVersionRankBase + token % divisor;
    }
    if (token >= kLegacySuffixThreshold) {
        return (token / kLegacySuffixDivisor) * kVersionRankBase + token % kLegacySuffixDivisor;
    }
    return token * kVersionRankBase;
}

std::optional<ParsedNumber> parse_runtime_token(const std::filesystem::path& path)
{
    const auto name = path_key(path.filename());
    constexpr std::string_view prefix = "cudart64_";
    constexpr std::string_view suffix = ".dll";
    if (!name.starts_with(prefix) || !name.ends_with(suffix)) {
        return std::nullopt;
    }
    const auto token = std::string_view(name).substr(prefix.size(), name.size() - prefix.size() - suffix.size());
    const auto parsed = parse_decimal_prefix(token);
    if (!parsed || parsed->consumed != token.size()) {
        return std::nullopt;
    }
    return parsed;
}

std::optional<int> parse_toolkit_rank(const std::string& key)
{
    const auto marker = key.find(kToolkitRootMarker);
    if (marker == std::string::npos) {
        return std::nullopt;
    }
    auto version = std::string_view(key).substr(marker + kToolkitRootMarker.size());
    if (!version.starts_with('v')) {
        return std::nullopt;
    }
    version.remove_prefix(1);
    const auto major = parse_decimal_prefix(version);
    if (!major) {
        return std::nullopt;
    }
    version.remove_prefix(major->consumed);
    int minor = 0;
    if (version.starts_with('.')) {
        minor = parse_decimal_prefix(version.substr(1)).value_or(ParsedNumber{}).value;
    }
    if (major->value > (std::numeric_limits<int>::max() - minor) / kVersionRankBase) {
        return std::nullopt;
    }
    return major->value * kVersionRankBase + minor;
}

PathKind classify_path(const std::string& parent)
{
    if (parent.find(kPhysXRootMarker) != std::string::npos) {
        return PathKind::PhysX;
    }
    if (parent.find(kToolkitRootMarker) == std::string::npos) {
        return PathKind::Generic;
    }
    if (parent.find("/bin/x64") != std::string::npos) {
        return PathKind::ToolkitBinX64;
    }
    return parent.find("/bin") != std::string::npos ? PathKind::ToolkitBin : PathKind::ToolkitRoot;
}
} // namespace

std::optional<Candidate> candidate_for_path(const std::filesystem::path& path, DirectorySource source)
{
    const auto lexical = windows_lexical_path(path);
    const auto token = parse_runtime_token(lexical);
    if (!token) {
        return std::nullopt;
    }
    const int version_rank = parse_toolkit_rank(path_key(lexical))
                                 .value_or(normalize_version_rank(token->value, token->consumed));
    return Candidate{path.lexically_normal(), token->value, version_rank,
                     source, classify_path(path_key(lexical.parent_path()))};
}

std::vector<Candidate> rank_candidates(std::vector<Candidate> candidates)
{
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs) {
        const auto lhs_priority = std::tuple(lhs.source, lhs.path_kind, -lhs.version_rank, -lhs.version_token);
        const auto rhs_priority = std::tuple(rhs.source, rhs.path_kind, -rhs.version_rank, -rhs.version_token);
        if (lhs_priority != rhs_priority) {
            return lhs_priority < rhs_priority;
        }
        return path_key(windows_lexical_path(lhs.path)) < path_key(windows_lexical_path(rhs.path));
    });
    return candidates;
}
} // namespace WindowsCudaRuntimeSelection
