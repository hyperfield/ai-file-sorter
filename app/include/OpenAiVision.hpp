/**
 * @file OpenAiVision.hpp
 * @brief Transport helpers for OpenAI-compatible multimodal chat requests.
 *
 * This layer only knows about image bytes and chat-completions JSON. Image
 * analysis policy (prompt wording, reply parsing, filename rules) lives in
 * ApiImageAnalyzer.
 */
#pragma once

#include <filesystem>
#include <string>

namespace OpenAiVision {

/** @brief Longest edge, in pixels, of an image sent to a vision endpoint. */
inline constexpr int kMaxImageLongEdgePixels = 2048;
/** @brief JPEG quality used for opaque images. */
inline constexpr int kJpegQuality = 85;

/**
 * @brief Normalized image ready to embed in a chat request.
 */
struct PreparedImage {
    /** @brief MIME type of the encoded bytes (image/jpeg or image/png). */
    std::string mime_type;
    /** @brief Complete `data:<mime>;base64,...` URL. Never log this value. */
    std::string data_url;
};

/**
 * @brief Decode an image of any Qt-readable format and encode it for transport.
 *
 * Orientation is applied from EXIF data, images whose long edge exceeds
 * kMaxImageLongEdgePixels are downscaled (aspect ratio preserved, never upscaled),
 * and the result is encoded as PNG when the image has alpha and JPEG otherwise.
 *
 * @param image_path Local image file.
 * @return Normalized image and its data URL.
 * @throws std::runtime_error when the image cannot be decoded or encoded.
 */
PreparedImage prepare_image_data_url(const std::filesystem::path& image_path);

/**
 * @brief Build a standard chat-completions request with one text and one image part.
 * @param model Model identifier sent to the server.
 * @param system_prompt System message content.
 * @param user_text User text part.
 * @param image_data_url Data URL produced by prepare_image_data_url().
 * @param max_tokens Reply token limit; omitted when not positive.
 * @param temperature Sampling temperature.
 * @param response_format_json OpenAI `response_format` object as JSON text; empty omits it.
 * @return Compact JSON request body.
 */
std::string build_image_chat_payload(const std::string& model,
                                     const std::string& system_prompt,
                                     const std::string& user_text,
                                     const std::string& image_data_url,
                                     int max_tokens,
                                     double temperature = 0.0,
                                     const std::string& response_format_json = {});

/**
 * @brief Final assistant message from a chat-completions response.
 */
struct AssistantMessage {
    /** @brief choices[0].message.content: the answer to use. */
    std::string content;
    /** @brief choices[0].message.reasoning_content when the server reports reasoning. */
    std::string reasoning_content;
};

/**
 * @brief Read choices[0].message from a chat-completions response body.
 *
 * Never returns reasoning text as the answer. When content is empty, the message object is
 * logged (truncated) and an error explains whether the answer was placed in reasoning_content.
 * @param response_body Raw response body.
 * @return Assistant message with non-empty content.
 * @throws std::runtime_error when the body is not JSON, has no message, or content is empty.
 */
AssistantMessage extract_assistant_message(const std::string& response_body);

} // namespace OpenAiVision
