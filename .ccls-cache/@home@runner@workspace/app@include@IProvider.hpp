#pragma once
#include "ProviderTypes.hpp"
#include <string>
#include <memory>

class IProvider {
public:
    virtual ~IProvider() = default;

    virtual ProviderKind kind() const = 0;

    virtual std::string display_name() const = 0;

    virtual std::string base_url() const = 0;

    virtual HealthResult check_health() = 0;

    virtual ListModelsResult list_models() = 0;

    virtual bool requires_api_key() const = 0;

    virtual bool has_api_key() const = 0;
};

using ProviderPtr = std::shared_ptr<IProvider>;
