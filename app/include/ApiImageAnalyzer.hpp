/**
 * @file ApiImageAnalyzer.hpp
 * @brief Image analysis through an OpenAI-compatible multimodal chat endpoint.
 *
 * Produces the same ImageAnalysisResult shape as the local visual backends (a description and
 * a suggested filename) so that downstream categorization is unaware of the backend. The
 * endpoint is only asked for description and filename; the category decision is made later.
 */
#pragma once

#include "ImageAnalyzer.hpp"
#include "Types.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class ILLMClient;

namespace ApiImageAnalyzer {

/** @brief Output budget for one description-and-filename reply. */
inline constexpr int kVisionMaxTokens = 512;

/**
 * @brief Parsed model reply before filename sanitization.
 */
struct VisionReply {
    std::string description;
    std::string suggested_name;
};

/**
 * @brief Resolves the endpoint selected as the image-analysis backend.
 * @param visual_model_id Persisted id in `api:<endpoint id>` form.
 * @param endpoints Configured custom API endpoints.
 * @param error Optional user-facing reason when resolution fails.
 * @return Endpoint that is complete and marked as supporting image input.
 */
std::optional<CustomApiEndpoint> resolve_visual_api_endpoint(std::string_view visual_model_id,
                                                              const std::vector<CustomApiEndpoint>& endpoints,
                                                              std::string* error = nullptr);

/**
 * @brief Returns the user-visible prompt sent alongside each image.
 */
std::string build_vision_prompt();

/**
 * @brief OpenAI `response_format` (json_schema, strict) for the description-and-filename reply.
 * @return JSON text to send as the request's response_format.
 */
std::string build_image_analysis_response_format();

/**
 * @brief Parses a reply of the form {"description": "...", "suggested_name": "..."}.
 *
 * Tries, in order: the whole reply as JSON, the reply without one Markdown code fence, and the
 * single top-level JSON object embedded in surrounding prose. Prose without a JSON object is
 * rejected. A missing description yields an empty string; a missing or non-string
 * suggested_name is an error. Free text is never treated as a name.
 * @throws std::runtime_error when no single JSON object can be read, or suggested_name is unusable.
 */
VisionReply parse_vision_reply(const std::string& content);

/**
 * @brief Analyzes one image through an LLM client that supports image input.
 *
 * Decoding, scaling and base64 encoding are done by the client transport. The suggested name is
 * sanitized with the same rules as local visual analysis and keeps the original extension.
 * @param llm Client owned by the calling worker.
 * @param image_path Local image file.
 * @return Description and filename suggestion.
 * @throws std::runtime_error on transport, decode, or reply failures (per-item errors).
 */
ImageAnalysisResult analyze_image(ILLMClient& llm, const std::filesystem::path& image_path);

} // namespace ApiImageAnalyzer
