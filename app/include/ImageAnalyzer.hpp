/**
 * @file ImageAnalyzer.hpp
 * @brief Generic image-analysis contracts for visual model backends.
 */
#pragma once

#include "FilenameWordLimits.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

inline constexpr int32_t kDefaultImageAnalyzerContextTokens = 4096;
inline constexpr int32_t kDefaultImageAnalyzerPredictTokens = 80;
inline constexpr float kDefaultImageAnalyzerTemperature = 0.2f;

/**
 * @brief Timing and batch diagnostics for a single visual inference pass.
 */
struct ImageInferenceDiagnostics {
    /** @brief Whether this pass included an image payload. */
    bool used_image = false;
    /** @brief Tokenization time in milliseconds. */
    double tokenize_ms = 0.0;
    /** @brief MTMD chunk evaluation time in milliseconds. */
    double eval_ms = 0.0;
    /** @brief Token generation time in milliseconds. */
    double generate_ms = 0.0;
    /** @brief Total time for this pass in milliseconds. */
    double total_ms = 0.0;
    /** @brief Last reported image batch index for this pass. */
    int32_t image_batch_current = 0;
    /** @brief Last reported total image batches for this pass. */
    int32_t image_batch_total = 0;
};

/**
 * @brief Detailed diagnostics for a full image-analysis request.
 */
struct ImageAnalysisDiagnostics {
    /** @brief Whether diagnostics were captured for this result. */
    bool available = false;
    /** @brief Bitmap decode/load time in milliseconds. */
    double bitmap_load_ms = 0.0;
    /** @brief End-to-end time for the full analysis request in milliseconds. */
    double total_ms = 0.0;
    /** @brief Effective llama context batch size used for evaluation. */
    int32_t batch_size = 0;
    /** @brief Whether the text model ran with GPU layers enabled. */
    bool text_gpu_enabled = false;
    /** @brief Whether the multimodal projector ran on GPU. */
    bool mmproj_gpu_enabled = false;
    /** @brief Description-pass timings. */
    ImageInferenceDiagnostics description_pass;
    /** @brief Filename-pass timings. */
    ImageInferenceDiagnostics filename_pass;
};

/**
 * @brief Result returned by an image analyzer.
 */
struct ImageAnalysisResult {
    /**
     * @brief Natural language description of the image contents.
     */
    std::string description;
    /**
     * @brief Suggested filename derived from the description.
     */
    std::string suggested_name;
    /**
     * @brief Optional runtime diagnostics for the analysis.
     */
    ImageAnalysisDiagnostics diagnostics;
};

/**
 * @brief Shared configuration for local image analyzers.
 */
struct ImageAnalyzerSettings {
    /** @brief Maximum words in a suggested filename, independent of its description. */
    size_t max_filename_words = kDefaultFilenameWords;
    /** @brief Context length (tokens). */
    int32_t n_ctx = kDefaultImageAnalyzerContextTokens;
    /** @brief Maximum tokens to predict. */
    int32_t n_predict = kDefaultImageAnalyzerPredictTokens;
    /** @brief Number of CPU threads to use (0 = auto). */
    int32_t n_threads = 0;
    /** @brief Sampling temperature. */
    float temperature = kDefaultImageAnalyzerTemperature;
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
 * @brief Polymorphic interface for visual backends that describe images.
 */
class ImageAnalyzer {
public:
    /**
     * @brief Virtual destructor for interface use.
     */
    virtual ~ImageAnalyzer() = default;

    /**
     * @brief Analyze an image and return a description and filename suggestion.
     * @param image_path Path to the image file.
     * @return Analysis result with description and suggested name.
     */
    virtual ImageAnalysisResult analyze(const std::filesystem::path& image_path) = 0;
};
