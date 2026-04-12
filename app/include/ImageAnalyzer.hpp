/**
 * @file ImageAnalyzer.hpp
 * @brief Generic image-analysis contracts for visual model backends.
 */
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

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
};

/**
 * @brief Shared configuration for local image analyzers.
 */
struct ImageAnalyzerSettings {
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
