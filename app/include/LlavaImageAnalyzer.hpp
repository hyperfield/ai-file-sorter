#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#ifdef AI_FILE_SORTER_HAS_MTMD
#include "ggml.h"

struct llama_model;
struct llama_context;
struct llama_vocab;
struct mtmd_context;
struct mtmd_bitmap;
#endif

/**
 * @brief Result returned by LlavaImageAnalyzer.
 */
struct LlavaImageAnalysisResult {
    /**
     * @brief Natural language description of the image contents.
     */
    std::string description;
    /**
     * @brief Suggested filename derived from the description.
     */
    std::string suggested_name;
};

/**
 * @brief Runs local LLaVA inference to describe images and suggest filenames.
 */
class LlavaImageAnalyzer {
public:
    /**
     * @brief Analyzer configuration for LLaVA inference.
     */
    struct Settings {
        /** @brief Context length (tokens). */
        int32_t n_ctx = 4096;
        /** @brief Maximum tokens to predict. */
        int32_t n_predict = 80;
        /** @brief Number of CPU threads to use (0 = auto). */
        int32_t n_threads = 0;
        /** @brief Sampling temperature. */
        float temperature = 0.2f;
        /** @brief Whether to use GPU acceleration. */
        bool use_gpu = true;
        /** @brief Enable verbose visual model logging. */
        bool log_visual_output = false;
        /**
         * @brief Optional callback for image batch progress.
         * @param current_batch Batch index (1-based).
         * @param total_batches Total number of batches.
         */
        std::function<void(int32_t current_batch, int32_t total_batches)> batch_progress;
    };

    /**
     * @brief Constructs the analyzer with explicit settings.
     * @param model_path Path to the LLaVA text model (GGUF).
     * @param mmproj_path Path to the LLaVA mmproj file (GGUF).
     * @param settings Inference settings.
     */
    LlavaImageAnalyzer(const std::filesystem::path& model_path,
                       const std::filesystem::path& mmproj_path,
                       Settings settings);
    /**
     * @brief Constructs the analyzer with default settings.
     * @param model_path Path to the LLaVA text model (GGUF).
     * @param mmproj_path Path to the LLaVA mmproj file (GGUF).
     */
    LlavaImageAnalyzer(const std::filesystem::path& model_path,
                       const std::filesystem::path& mmproj_path);
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
    LlavaImageAnalysisResult analyze(const std::filesystem::path& image_path);

    /**
     * @brief Returns true if the image path has a supported extension.
     * @param path Path to inspect.
     * @return True when the file is supported.
     */
    static bool is_supported_image(const std::filesystem::path& path);

private:
    /**
     * @brief Builds the description prompt for the visual model.
     * @return Prompt string.
     */
    std::string build_description_prompt() const;
    /**
     * @brief Builds the filename prompt from an image description.
     * @param description Model-generated description.
     * @return Prompt string.
     */
    std::string build_filename_prompt(const std::string& description) const;
#ifdef AI_FILE_SORTER_HAS_MTMD
    /**
     * @brief Runs inference on the given bitmap.
     * @param bitmap Input bitmap.
     * @param prompt Prompt to run.
     * @param max_tokens Maximum tokens to generate.
     * @return Model response text.
     */
    std::string infer_text(mtmd_bitmap* bitmap,
                           const std::string& prompt,
                           int32_t max_tokens);
#else
    /**
     * @brief Runs inference on the given bitmap (stub for non-MTMD builds).
     * @param bitmap Input bitmap.
     * @param prompt Prompt to run.
     * @param max_tokens Maximum tokens to generate.
     * @return Model response text.
     */
    std::string infer_text(void* bitmap,
                           const std::string& prompt,
                           int32_t max_tokens);
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
    int32_t batch_size_{512};
    bool text_gpu_enabled_{false};
    bool mmproj_gpu_enabled_{false};
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
};
