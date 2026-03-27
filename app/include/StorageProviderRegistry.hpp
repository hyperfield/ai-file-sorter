#pragma once

#include "StorageProvider.hpp"

#include <memory>
#include <vector>

/**
 * @brief Stores built-in providers and resolves which one should handle a path.
 */
class StorageProviderRegistry {
public:
    void register_builtin(std::shared_ptr<IStorageProvider> provider);

    StorageProviderDetection detect(const std::string& root_path) const;
    std::shared_ptr<IStorageProvider> find_by_id(const std::string& provider_id) const;
    std::shared_ptr<IStorageProvider> resolve_for(const std::string& root_path) const;

private:
    std::vector<std::shared_ptr<IStorageProvider>> providers_;
};
