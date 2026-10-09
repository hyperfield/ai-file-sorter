#pragma once
#include "Types.hpp"
#include <stdexcept>
#include <string>

class ILLMClient {
public:
    virtual ~ILLMClient() = default;
    virtual std::string categorize_file(const std::string& file_name,
                                        const std::string& file_path,
                                        FileType file_type,
                                        const std::string& consistency_context) = 0;
    virtual std::string complete_prompt(const std::string& prompt,
                                        int max_tokens) = 0;
    virtual void set_prompt_logging_enabled(bool enabled) = 0;

    /**
     * @brief Returns true when complete_prompt_with_image() is implemented by this client.
     */
    virtual bool supports_image_input() const { return false; }

    /**
     * @brief Generation settings for a multimodal completion.
     */
    struct ImageCompletionOptions {
        /** @brief Reply token limit; omitted from the request when not positive. */
        int max_tokens{0};
        /** @brief Sampling temperature sent with the request. */
        double temperature{0.0};
        /** @brief OpenAI `response_format` object as JSON text; empty sends none. */
        std::string response_format_json;
    };

    /**
     * @brief Sends a text prompt together with an image and returns the assistant message content.
     * @param prompt User prompt text.
     * @param image_path Local image file; the implementation decodes and encodes it.
     * @param options Generation settings.
     * @throws std::runtime_error when the client has no multimodal transport, or the reply has no content.
     */
    virtual std::string complete_prompt_with_image(const std::string& prompt,
                                                   const std::string& image_path,
                                                   const ImageCompletionOptions& options)
    {
        (void)prompt;
        (void)image_path;
        (void)options;
        throw std::runtime_error("This LLM client does not support image input.");
    }
};
