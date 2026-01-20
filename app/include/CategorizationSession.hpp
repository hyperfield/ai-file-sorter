#ifndef CATEGORIZATIONSESSION_HPP
#define CATEGORIZATIONSESSION_HPP

#include <LLMClient.hpp>
#include <string>

class CategorizationSession {
    std::string key;
    std::string model;
    std::string base_url;

public:
    /**
     * @brief Construct a session for OpenAI-compatible requests.
     */
    CategorizationSession(std::string api_key, std::string model, std::string base_url = std::string());
    ~CategorizationSession();

    LLMClient create_llm_client() const;
};

#endif
