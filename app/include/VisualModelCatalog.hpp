/**
 * @file VisualModelCatalog.hpp
 * @brief Descriptor catalog for supported local visual model backends.
 */
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief Supported local visual inference stacks.
 */
enum class VisualModelArchitecture {
    MtmdProjector,
};

/**
 * @brief Prompt policy used when talking to a visual backend.
 */
enum class VisualPromptPolicy {
    /** @brief Legacy prompt wording tuned for the LLaVA 1.6 family. */
    LegacyLlava,
    /** @brief General instruction-tuned multimodal prompt wording. */
    StructuredVisionInstruct,
};

/**
 * @brief Artifact kinds required by visual model backends.
 */
enum class VisualModelArtifactKind {
    Model,
    Mmproj,
};

/**
 * @brief Descriptor for a required visual model artifact.
 */
struct VisualModelArtifactDescriptor {
    /** @brief Artifact kind used by runtime/factory code. */
    VisualModelArtifactKind kind;
    /** @brief User-facing artifact name. */
    const char* display_name;
    /** @brief Environment variable containing the artifact download URL. */
    const char* url_env;
    /** @brief Stable backend-specific storage filename used for new downloads. */
    const char* local_storage_name;
    /** @brief Optional fallback filenames searched in the default model directory. */
    std::vector<std::string_view> fallback_filenames;
};

/**
 * @brief Descriptor for a supported visual model backend.
 */
struct VisualModelDescriptor {
    /** @brief Stable backend identifier. */
    const char* id;
    /** @brief User-facing backend display name. */
    const char* display_name;
    /** @brief Architecture family used to instantiate the analyzer. */
    VisualModelArchitecture architecture;
    /** @brief Prompt policy used for image description / rename prompts. */
    VisualPromptPolicy prompt_policy;
    /** @brief Required artifacts for the backend. */
    std::vector<VisualModelArtifactDescriptor> artifacts;
};

/**
 * @brief Return all built-in visual model descriptors.
 * @return Descriptor list in priority order.
 */
const std::vector<VisualModelDescriptor>& visual_model_descriptors();

/**
 * @brief Find a built-in visual model descriptor by id.
 * @param id Stable backend identifier to resolve.
 * @return Matching descriptor, or nullptr when not found.
 */
const VisualModelDescriptor* find_visual_model_descriptor(std::string_view id);

/**
 * @brief Return the currently preferred built-in visual model descriptor.
 * @return Default visual model descriptor.
 */
const VisualModelDescriptor& default_visual_model_descriptor();

/**
 * @brief Return the generic descriptor used for user-provided visual GGUF/MMProj pairs.
 * @return Descriptor with the default custom visual runtime policy.
 */
const VisualModelDescriptor& custom_visual_model_descriptor();

/**
 * @brief Build a persisted visual model id for a custom local LLM.
 * @param custom_llm_id Custom LLM id.
 * @return Visual model id in `custom:<id>` form, or empty when the id is empty.
 */
std::string custom_visual_model_id_for_llm(std::string_view custom_llm_id);

/**
 * @brief Extract a custom LLM id from a persisted custom visual model id.
 * @param visual_model_id Visual model id to parse.
 * @return Custom LLM id when the value uses the `custom:<id>` form.
 */
std::optional<std::string> custom_llm_id_from_visual_model_id(std::string_view visual_model_id);

/**
 * @brief Return whether a visual model id references a custom local LLM.
 * @param visual_model_id Visual model id to inspect.
 * @return True when the id uses the `custom:<id>` form.
 */
bool is_custom_visual_model_id(std::string_view visual_model_id);

/**
 * @brief Return the canonical on-disk location for a visual model artifact.
 * @param backend Backend descriptor owning the artifact.
 * @param artifact Artifact descriptor to resolve.
 * @return Backend-specific storage path for new downloads.
 */
std::filesystem::path visual_artifact_storage_path(const VisualModelDescriptor& backend,
                                                   const VisualModelArtifactDescriptor& artifact);

/**
 * @brief Resolve an existing visual artifact, including legacy flat-file installs.
 * @param backend Backend descriptor owning the artifact.
 * @param artifact Artifact descriptor to resolve.
 * @param download_url Download URL currently configured for the artifact.
 * @return Resolved on-disk artifact path when available.
 */
std::optional<std::filesystem::path> resolve_visual_artifact_path(
    const VisualModelDescriptor& backend,
    const VisualModelArtifactDescriptor& artifact,
    std::string_view download_url);
