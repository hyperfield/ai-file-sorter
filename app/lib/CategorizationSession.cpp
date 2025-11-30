#include "CategorizationSession.hpp"

#include <algorithm>
#include <utility>


CategorizationSession::CategorizationSession(std::string api_key, std::string model)
    : key(std::move(api_key)),
      model(std::move(model))
{
}


CategorizationSession::~CategorizationSession()
{
    // Securely clear key memory
    std::fill(key.begin(), key.end(), '\0');
}


LLMClient CategorizationSession::create_llm_client() const
{
    return LLMClient(key, model);
}
