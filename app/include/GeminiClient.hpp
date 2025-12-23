#ifndef GEMINICLIENT_HPP
#define GEMINICLIENT_HPP

#include "ILLMClient.hpp"
#include <Types.hpp>
#include <string>

class GeminiClient : public ILLMClient {
public:
    GeminiClient(std::string api_key, std::string model);
    ~GeminiClient() override = default;

    std::string categorize_file(const std::string& file_name,
                                const std::string& file_path,
                                FileType file_type,
                                const std::string& consistency_context) override;
    std::string complete_prompt(const std::string& prompt,
                                int max_tokens) override;
    void set_prompt_logging_enabled(bool enabled) override;

private:
    std::string api_key_;
    std::string model_;
    bool prompt_logging_enabled_{false};
    std::string last_prompt_;

    std::string send_api_request(const std::string& json_payload);
    std::string make_categorization_payload(const std::string& file_name,
                                            const std::string& file_path,
                                            FileType file_type,
                                            const std::string& consistency_context);
    std::string make_generic_payload(const std::string& system_prompt,
                                     const std::string& user_prompt,
                                     int max_tokens) const;
    std::string effective_model() const;
};

#endif // GEMINICLIENT_HPP
