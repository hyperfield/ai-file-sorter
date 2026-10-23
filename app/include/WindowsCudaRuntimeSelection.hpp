#pragma once

#include <filesystem>
#include <optional>
#include <vector>

/** @brief Pure Windows CUDA runtime selection policy, independent of host OS and DLL loading. */
namespace WindowsCudaRuntimeSelection {

/** @brief Discovery source, ordered from highest to lowest priority. */
enum class DirectorySource {
    ToolkitHint,
    Path,
};

/** @brief Runtime directory layout, ordered from highest to lowest priority. */
enum class PathKind {
    ToolkitBinX64,
    ToolkitBin,
    ToolkitRoot,
    Generic,
    PhysX,
};

/** @brief Parsed runtime candidate with the metadata needed for deterministic selection. */
struct Candidate {
    std::filesystem::path path;
    int version_token{0};
    int version_rank{0};
    DirectorySource source{DirectorySource::Path};
    PathKind path_kind{PathKind::Generic};
};

/**
 * @brief Parse a CUDA runtime DLL path using Windows separator and ASCII case rules.
 * @param path Candidate path; no filesystem access is performed.
 * @param source Discovery source used to prioritize the candidate.
 * @return Parsed candidate, or std::nullopt for an invalid DLL filename.
 */
std::optional<Candidate> candidate_for_path(
    const std::filesystem::path& path, DirectorySource source = DirectorySource::Path);

/**
 * @brief Rank parsed candidates by source, layout, version, and normalized path.
 * @param candidates Candidates to sort; paths are not loaded or inspected on disk.
 * @return Candidates in preferred selection order.
 */
std::vector<Candidate> rank_candidates(std::vector<Candidate> candidates);

} // namespace WindowsCudaRuntimeSelection
