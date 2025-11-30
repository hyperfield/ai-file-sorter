#ifndef CATEGORIZATIONSESSION_HPP
#define CATEGORIZATIONSESSION_HPP

#include <LLMClient.hpp>
#include <string>

class CategorizationSession {
    std::string key;
    std::string model;

public:
    CategorizationSession(std::string api_key, std::string model);
    ~CategorizationSession();

    LLMClient create_llm_client() const;
};

#endif
