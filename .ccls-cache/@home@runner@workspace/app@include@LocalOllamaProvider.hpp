#pragma once
#include "IProvider.hpp"
#include <string>
#include <memory>

namespace spdlog { class logger; }

class LocalOllamaProvider : public IProvider {
public:
    explicit LocalOllamaProvider(std::string base_url = "http://localhost:11434");

    ProviderKind kind() const override;
    std::string display_name() const override;
    std::string base_url() const override;
    HealthResult check_health() override;
    ListModelsResult list_models() override;
    bool requires_api_key() const override;
    bool has_api_key() const override;

    void set_timeout(int seconds);

private:
    std::string base_url_;
    int timeout_seconds_ = 10;
    std::shared_ptr<spdlog::logger> logger_;

    std::string perform_get(const std::string& endpoint, long& http_code);
    std::vector<OllamaModel> parse_tags_response(const std::string& json_body);
};
