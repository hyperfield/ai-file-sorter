#pragma once

#include "ChatCompletionTransport.hpp"
#include "ImageAnalyzer.hpp"
#include "Types.hpp"

/** @brief Analyze images using a user-selected OpenAI-compatible vision server. */
class HttpImageAnalyzer final : public ImageAnalyzer {
public:
    /**
     * @brief Configure a vision-capable API connection.
     * @param endpoint Explicitly enabled vision connection with URL and model id.
     * @param settings Filename policy, logging and cancellation settings.
     */
    HttpImageAnalyzer(CustomApiEndpoint endpoint, ImageAnalyzerSettings settings = {});

    /**
     * @brief Upload image pixels for description, then request a filename from that description.
     * @param image_path Image to decode and analyze.
     * @return Description and a safe filename preserving the original extension.
     */
    ImageAnalysisResult analyze(const std::filesystem::path& image_path) override;

private:
    CustomApiEndpoint endpoint_;
    ImageAnalyzerSettings settings_;
    ChatCompletionTransport transport_;
};
