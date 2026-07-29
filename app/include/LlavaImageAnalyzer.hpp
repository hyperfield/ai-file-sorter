#pragma once

#include "ImageAnalyzer.hpp"
#include "VisualModelCatalog.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef AI_FILE_SORTER_HAS_MTMD
#include "ggml.h"

struct llama_model;
struct llama_context;
struct llama_vocab;
struct mtmd_context;
struct mtmd_bitmap;
#endif

using LlavaImageAnalysisResult = ImageAnalysisResult;

/**
 * @brief Runs local MTMD-backed visual inference to describe images and suggest filenames.
 *
 * The class keeps its historical name, but it now serves any supported local
 * visual backend that uses the current text-model-plus-mmproj runtime path.
 */
class LlavaImageAnalyzer final : public ImageAnalyzer {
public:
    using Settings = ImageAnalyzerSettings;

    /**
     * @brief Constructs the analyzer with explicit settings.
     * @param model_path Path to the visual text model (GGUF).
     * @param mmproj_path Path to the matching multimodal projector file (GGUF).
     * @param prompt_policy Backend-specific prompt policy.
     * @param settings Inference settings.
     */
    LlavaImageAnalyzer(const std::filesystem::path& model_path,
                       const std::filesystem::path& mmproj_path,
                       VisualPromptPolicy prompt_policy,
                       Settings settings);
    /**
     * @brief Constructs the analyzer with default settings.
     * @param model_path Path to the visual text model (GGUF).
     * @param mmproj_path Path to the matching multimodal projector file (GGUF).
     * @param prompt_policy Backend-specific prompt policy.
     */
    LlavaImageAnalyzer(const std::filesystem::path& model_path,
                       const std::filesystem::path& mmproj_path,
                       VisualPromptPolicy prompt_policy = VisualPromptPolicy::LegacyLlava);
    /**
     * @brief Destructor; releases model resources.
     */
    ~LlavaImageAnalyzer();

    LlavaImageAnalyzer(const LlavaImageAnalyzer&) = delete;
    LlavaImageAnalyzer& operator=(const LlavaImageAnalyzer&) = delete;

    /**
     * @brief Analyze an image and return description + filename suggestion.
     * @param image_path Path to the image file.
     * @return Analysis result with description and suggested name.
     */
    ImageAnalysisResult analyze(const std::filesystem::path& image_path) override;

    /**
     * @brief Returns true if the image path has a supported extension.
     * @param path Path to inspect.
     * @return True when the file is supported.
     */
    static bool is_supported_image(const std::filesystem::path& path);

private:
#ifdef AI_FILE_SORTER_HAS_MTMD
    /**
     * @brief Runs inference on the given bitmap.
     * @param bitmap Input bitmap.
     * @param system_prompt Optional system prompt to apply via chat template.
     * @param user_prompt Prompt to run.
     * @param max_tokens Maximum tokens to generate.
     * @return Model response text.
     */
    std::string infer_text(mtmd_bitmap* bitmap,
                           std::string_view system_prompt,
                           const std::string& user_prompt,
                           int32_t max_tokens,
                           ImageInferenceDiagnostics* diagnostics = nullptr);
#else
    /**
     * @brief Runs inference on the given bitmap (stub for non-MTMD builds).
     * @param bitmap Input bitmap.
     * @param system_prompt Optional system prompt to apply via chat template.
     * @param user_prompt Prompt to run.
     * @param max_tokens Maximum tokens to generate.
     * @return Model response text.
     */
    std::string infer_text(void* bitmap,
                           std::string_view system_prompt,
                           const std::string& user_prompt,
                           int32_t max_tokens,
                           ImageInferenceDiagnostics* diagnostics = nullptr);
#endif
    /**
     * @brief Sanitizes a suggested filename.
     * @param value Raw suggested filename.
     * @param max_words Max number of words.
     * @param max_length Max character length.
     * @return Sanitized filename.
     */
    std::string sanitize_filename(const std::string& value,
                                  size_t max_words,
                                  size_t max_length) const;

    /**
     * @brief Trims whitespace from both ends of a string.
     * @param value Input string.
     * @return Trimmed string.
     */
    static std::string trim(std::string value);
    /**
     * @brief Converts a string into a slug safe for filenames.
     * @param value Input string.
     * @return Slugified string.
     */
    static std::string slugify(const std::string& value);
    /**
     * @brief Normalizes a filename based on the original image path.
     * @param base Base filename.
     * @param original_path Path to the original image.
     * @return Normalized filename.
     */
    static std::string normalize_filename(const std::string& base,
                                          const std::filesystem::path& original_path);

#ifdef AI_FILE_SORTER_HAS_MTMD
    llama_model* model_{nullptr};
    llama_context* context_{nullptr};
    mtmd_context* vision_ctx_{nullptr};
    const llama_vocab* vocab_{nullptr};
    std::filesystem::path model_path_;
    std::filesystem::path mmproj_path_;
    std::optional<bool> visual_gpu_override_;
    int32_t context_tokens_{0};
    int32_t batch_size_{0};
    bool text_gpu_enabled_{false};
    bool mmproj_gpu_enabled_{false};
    std::atomic<int32_t> image_batch_current_{0};
    std::atomic<int32_t> image_batch_total_{0};
    void initialize_context();
    void reset_context_state();
    static void mtmd_progress_callback(const char* name,
                                       int32_t current_batch,
                                       int32_t total_batches,
                                       void* user_data);
#if defined(AI_FILE_SORTER_MTMD_LOG_CALLBACK)
    /**
     * @brief Optional MTMD log callback.
     * @param level Log level.
     * @param text Log message.
     * @param user_data User data pointer.
     */
    static void mtmd_log_callback(enum ggml_log_level level,
                                  const char* text,
                                  void* user_data);
#endif
#endif
    /**
     * @brief Stored analyzer settings.
     */
    Settings settings_;
    /** @brief Backend-specific prompt policy. */
    VisualPromptPolicy prompt_policy_{VisualPromptPolicy::LegacyLlava};
};

#ifdef AI_FILE_SORTER_TEST_BUILD
namespace LlavaImageAnalyzerTestAccess {
int32_t default_visual_batch_size(bool gpu_enabled, std::string_view backend_name);
int32_t visual_model_n_gpu_layers_for_model(const std::string& model_path);
/**
 * @brief Builds the visual GPU-layer retry ladder from a headroom-aware starting point.
 * @param initial_layers Initial visual `n_gpu_layers` value to try.
 * @return Descending retry candidates, including the starting point when positive.
 */
std::vector<int32_t> visual_gpu_layer_retry_candidates(int32_t initial_layers);
/**
 * @brief Estimates a safer visual n_gpu_layers cap after reserving mmproj and eval headroom.
 * @param model_path Path to the visual text model.
 * @param mmproj_path Path to the multimodal projector model.
 * @param backend_name Active backend label such as `cuda` or `vulkan`.
 * @param free_bytes Free GPU memory in bytes before loading the visual model.
 * @param total_bytes Total GPU memory in bytes.
 * @return Recommended visual n_gpu_layers cap; `0` when GPU headroom is insufficient and `-1`
 *         when the backend does not require visual headroom capping.
 */
int32_t visual_model_n_gpu_layers_with_headroom(const std::string& model_path,
                                                const std::string& mmproj_path,
                                                std::string_view backend_name,
                                                size_t free_bytes,
                                                size_t total_bytes);
/**
 * @brief Evaluates whether a visual projector should remain on GPU for the given free memory.
 * @param backend_name Active GPU backend name.
 * @param free_bytes Free bytes reported after loading the visual text model.
 * @param mmproj_file_size Multimodal projector file size in bytes.
 * @return True when the projector can use GPU memory for that backend.
 */
bool should_use_mmproj_gpu_for_memory(std::string_view backend_name,
                                      size_t free_bytes,
                                      std::uintmax_t mmproj_file_size);
std::string description_system_prompt(VisualPromptPolicy policy);
std::string description_user_prompt(VisualPromptPolicy policy);
std::string filename_system_prompt(VisualPromptPolicy policy);
std::string filename_user_prompt(VisualPromptPolicy policy, std::string_view description);
/**
 * @brief Checks whether a WebP file can be decoded and transcoded for MTMD input.
 * @param path WebP image path to decode.
 * @return True when the file can be decoded and encoded as PNG bytes.
 */
bool can_transcode_webp_for_visual_analysis(const std::filesystem::path& path);
}
#endif
