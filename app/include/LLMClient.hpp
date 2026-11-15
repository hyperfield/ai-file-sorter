#ifndef LLMCLIENT_HPP
#define LLMCLIENT_HPP

#include "ILLMClient.hpp"
#include <Types.hpp>
#include <cstddef>
#include <string>

class LLMClient : public ILLMClient {
public:
    /**
     * @brief Create an OpenAI-compatible client, optionally targeting a custom base URL.
     */
    LLMClient(std::string api_key, std::string model, std::string base_url = std::string());
    ~LLMClient() override;
    std::string categorize_file(const std::string& file_name,
                                const std::string& file_path,
                                FileType file_type,
                                const std::string& consistency_context) override;
    std::string complete_prompt(const std::string& prompt,
                                int max_tokens) override;
    void set_prompt_logging_enabled(bool enabled) override;

private:
    std::string api_key;
    std::string send_api_request(std::string json_payload);
    std::string make_payload(const std::string &file_name,
                             const std::string &file_path,
                                const FileType file_type,
                                const std::string& consistency_context);
    std::string make_generic_payload(const std::string& system_prompt,
                                     const std::string& user_prompt,
                                     int max_tokens) const;
    std::string effective_model() const;
    /**
     * @brief Resolve the final /chat/completions URL from the base URL or default.
     */
    std::string resolve_api_url() const;
    /**
     * @brief Add one request's token usage (from the API's usage block, or an
     * llm-cost estimate when absent) to this client's running spend meter.
     */
    void record_usage(const std::string& request_payload,
                      const std::string& reply,
                      std::size_t prompt_tokens,
                      std::size_t completion_tokens,
                      bool reported_by_api);
    struct UsageTotals {
        std::size_t requests{0};
        std::size_t prompt_tokens{0};
        std::size_t completion_tokens{0};
        double cost_usd{0.0};
    };
    UsageTotals usage_totals;
    bool prompt_logging_enabled{false};
    std::string last_prompt;
    std::string model;
    std::string base_url;
};

#endif
