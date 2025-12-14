#pragma once
#include "IProvider.hpp"
#include <string>
#include <optional>
#include <memory>

namespace spdlog { class logger; }

class OllamaCloudProvider : public IProvider {
public:
    explicit OllamaCloudProvider(std::string base_url = "");

    ProviderKind kind() const override;
    std::string display_name() const override;
    std::string base_url() const override;
    HealthResult check_health() override;
    ListModelsResult list_models() override;
    bool requires_api_key() const override;
    bool has_api_key() const override;

    void set_timeout(int seconds);
    void set_api_key(const std::string& key);

    static std::optional<std::string> get_api_key_from_env();

private:
    std::string base_url_;
    std::string api_key_;
    int timeout_seconds_ = 30;
    std::shared_ptr<spdlog::logger> logger_;

    std::string perform_get(const std::string& endpoint, long& http_code);
    std::vector<OllamaModel> parse_tags_response(const std::string& json_body);
    std::string resolve_api_key() const;
};
