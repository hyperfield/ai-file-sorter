#include "StorageProviderRegistry.hpp"

#include <algorithm>

void StorageProviderRegistry::register_builtin(std::shared_ptr<IStorageProvider> provider)
{
    if (!provider) {
        return;
    }

    providers_.erase(std::remove_if(providers_.begin(),
                                    providers_.end(),
                                    [&provider](const std::shared_ptr<IStorageProvider>& existing) {
                                        return existing && existing->id() == provider->id();
                                    }),
                     providers_.end());
    providers_.push_back(std::move(provider));
}

void StorageProviderRegistry::clear()
{
    providers_.clear();
}

StorageProviderDetection StorageProviderRegistry::detect(const std::string& root_path) const
{
    StorageProviderDetection best_detection;

    for (const auto& provider : providers_) {
        if (!provider) {
            continue;
        }

        const auto detection = provider->detect(root_path);
        if (!detection.matched) {
            continue;
        }

        if (!best_detection.matched || detection.confidence > best_detection.confidence) {
            best_detection = detection;
        }
    }

    return best_detection;
}

std::shared_ptr<IStorageProvider> StorageProviderRegistry::find_by_id(const std::string& provider_id) const
{
    for (const auto& provider : providers_) {
        if (provider && provider->id() == provider_id) {
            return provider;
        }
    }
    return nullptr;
}

std::shared_ptr<IStorageProvider> StorageProviderRegistry::resolve_for(const std::string& root_path) const
{
    std::shared_ptr<IStorageProvider> best_provider;
    int best_confidence = -1;

    for (const auto& provider : providers_) {
        if (!provider) {
            continue;
        }

        const auto detection = provider->detect(root_path);
        if (!detection.matched || detection.needs_additional_support) {
            continue;
        }

        if (!best_provider || detection.confidence > best_confidence) {
            best_provider = provider;
            best_confidence = detection.confidence;
        }
    }

    if (best_provider) {
        return best_provider;
    }
    if (!providers_.empty()) {
        return providers_.front();
    }
    return nullptr;
}
