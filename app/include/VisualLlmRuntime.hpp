#pragma once

#include "Types.hpp"
#include "VisualModelCatalog.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

/**
 * @brief Resolves local visual/text LLM runtime files and fallback heuristics.
 */
class VisualLlmRuntime {
public:
    /**
     * @brief Resolved filesystem path for a visual model artifact.
     */
    struct ResolvedArtifact {
        /** @brief Descriptor for the resolved artifact. */
        const VisualModelArtifactDescriptor* descriptor{nullptr};
        /** @brief Filesystem path to the resolved artifact. */
        std::filesystem::path path;
    };

    /**
     * @brief Resolved backend descriptor and artifact paths.
     */
    struct Backend {
        /** @brief Descriptor for the selected backend. */
        const VisualModelDescriptor* descriptor{nullptr};
        /** @brief Resolved artifact paths for the backend. */
        std::vector<ResolvedArtifact> artifacts;

        /**
         * @brief Return the resolved path for the given artifact kind.
         * @param kind Artifact kind to resolve.
         * @return Artifact path when present; otherwise std::nullopt.
         */
        std::optional<std::filesystem::path> path_for(VisualModelArtifactKind kind) const;
    };

    /**
     * @brief Convenience view for current two-artifact local visual backends.
     */
    struct Paths {
        std::filesystem::path model_path;
        std::filesystem::path mmproj_path;
    };

    static bool default_text_llm_files_available();
    static std::optional<Backend> resolve_active_backend(std::string_view backend_id = {},
                                                         std::string* error = nullptr);
    /**
     * @brief Resolve a built-in or custom visual backend by id.
     * @param backend_id Built-in descriptor id, custom visual id, or empty for default.
     * @param custom_llms Custom local LLM entries available for `custom:<id>` resolution.
     * @param error Optional error output.
     * @return Resolved backend when required artifacts are available.
     */
    static std::optional<Backend> resolve_active_backend(
        std::string_view backend_id,
        const std::vector<CustomLLM>& custom_llms,
        std::string* error = nullptr);
    static std::optional<Paths> resolve_paths(std::string_view backend_id = {},
                                              std::string* error = nullptr);
    /**
     * @brief Resolve the model and MMProj paths for a built-in or custom backend.
     * @param backend_id Built-in descriptor id, custom visual id, or empty for default.
     * @param custom_llms Custom local LLM entries available for `custom:<id>` resolution.
     * @param error Optional error output.
     * @return Model and MMProj paths when both are available.
     */
    static std::optional<Paths> resolve_paths(std::string_view backend_id,
                                              const std::vector<CustomLLM>& custom_llms,
                                              std::string* error = nullptr);
    static bool should_use_gpu();
    static bool should_offer_cpu_fallback(const std::string& reason);
};
