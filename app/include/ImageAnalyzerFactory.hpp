/**
 * @file ImageAnalyzerFactory.hpp
 * @brief Factory for creating analyzer instances from resolved visual backends.
 */
#pragma once

#include "ImageAnalyzer.hpp"
#include "VisualLlmRuntime.hpp"

#include <memory>

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
