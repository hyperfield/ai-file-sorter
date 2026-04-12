/**
 * @file VisualModelCatalog.hpp
 * @brief Descriptor catalog for supported local visual model backends.
 */
#pragma once

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
