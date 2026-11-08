/**
 * @file ImageAnalyzerFactory.hpp
 * @brief Factory for creating analyzer instances from resolved visual backends.
 */
#pragma once

#include "ImageAnalyzer.hpp"
#include "VisualLlmRuntime.hpp"

#include <memory>

#if defined(AI_FILE_SORTER_TEST_BUILD) && (defined(_WIN32) || defined(__APPLE__))
class QProcessEnvironment;
#endif

/**
 * @brief Creates image analyzers for resolved visual model backends.
 */
class ImageAnalyzerFactory {
public:
    /**
     * @brief Create an analyzer for the resolved visual backend.
     * @param backend Resolved backend descriptor and artifact paths.
     * @param settings Analyzer settings to apply.
     * @return Analyzer instance ready for inference.
     */
    static std::unique_ptr<ImageAnalyzer> create(const VisualLlmRuntime::Backend& backend,
                                                 ImageAnalyzerSettings settings = {});
};

#if defined(AI_FILE_SORTER_TEST_BUILD) && (defined(_WIN32) || defined(__APPLE__))
/**
 * @brief Test hooks for visual analyzer factory internals.
 */
namespace ImageAnalyzerFactoryTestAccess {
/**
 * @brief Build the environment passed to the visual GPU preflight subprocess.
 * @return Process environment used by the preflight child process.
 */
QProcessEnvironment visual_gpu_preflight_environment();
} // namespace ImageAnalyzerFactoryTestAccess
#endif
