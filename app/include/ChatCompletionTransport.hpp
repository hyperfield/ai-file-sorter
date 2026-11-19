#pragma once

#include <functional>
#include <stdexcept>
#include <string>

/** @brief Raised when an HTTP completion is cancelled by its caller. */
class ChatCompletionCancelled : public std::runtime_error {
public:
    /** @brief Construct the cancellation diagnostic. */
    ChatCompletionCancelled() : std::runtime_error("API request cancelled.") {}
};

/** @brief HTTP transport shared by text and image chat-completion clients. */
class ChatCompletionTransport {
public:
    /**
     * @brief Configure a completion endpoint.
     * @param api_key Optional bearer token.
     * @param base_url Base URL or full chat/completions URL; empty selects OpenAI.
     */
    ChatCompletionTransport(std::string api_key, std::string base_url);

    /**
     * @brief Post a JSON request and extract the assistant's text.
     * @param payload Serialized chat-completion request.
     * @param should_cancel Optional cancellation probe, called on the requesting thread.
     * @return Nonempty assistant text; transport, HTTP and response errors throw.
     */
    std::string complete(const std::string& payload,
                         const std::function<bool()>& should_cancel = {}) const;

private:
    std::string api_key_;
    std::string base_url_;
};
