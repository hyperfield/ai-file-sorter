#include "CloudCompatibilityProvider.hpp"

#include "CloudPathSupport.hpp"

CloudCompatibilityProvider::CloudCompatibilityProvider(std::string provider_id,
                                                       std::string display_name,
                                                       std::vector<std::string> path_markers,
                                                       std::vector<std::string> env_var_names,
                                                       FileScannerBehavior scan_behavior,
                                                       StorageProviderCapabilities capabilities)
    : provider_id_(std::move(provider_id)),
      display_name_(std::move(display_name)),
      path_markers_(std::move(path_markers)),
      env_var_names_(std::move(env_var_names)),
      capabilities_(capabilities),
      scan_behavior_(std::move(scan_behavior))
{
    if (!capabilities_.supports_online_only_files) {
        capabilities_.supports_online_only_files = true;
    }
    capabilities_.supports_atomic_rename = false;
    if (!capabilities_.should_skip_reparse_points) {
        capabilities_.should_skip_reparse_points = true;
    }
    if (!capabilities_.should_relax_undo_mtime_validation) {
        capabilities_.should_relax_undo_mtime_validation = true;
    }

    if (!scan_behavior_.skip_reparse_points) {
        scan_behavior_.skip_reparse_points = capabilities_.should_skip_reparse_points;
    }
}

std::string CloudCompatibilityProvider::id() const
{
    return provider_id_;
}

StorageProviderDetection CloudCompatibilityProvider::detect(const std::string& root_path) const
{
    const CloudPathMatch match = detect_cloud_path_match(root_path, path_markers_, env_var_names_);
    if (!match.matched || match.confidence <= 0) {
        return {};
    }

    return StorageProviderDetection{
        .provider_id = provider_id_,
        .matched = true,
        .needs_additional_support = false,
        .confidence = match.confidence + 20,
        .message = "Detected a " + display_name_ + " folder. Compatibility support is active."
    };
}

StorageProviderCapabilities CloudCompatibilityProvider::capabilities() const
{
    return capabilities_;
}

std::vector<FileEntry> CloudCompatibilityProvider::list_directory(const std::string& directory,
                                                                  FileScanOptions options) const
{
    return scanner_.get_directory_entries(directory, options, scan_behavior_);
}

bool CloudCompatibilityProvider::path_exists(const std::string& path) const
{
    return fallback_provider_.path_exists(path);
}

bool CloudCompatibilityProvider::ensure_directory(const std::string& directory, std::string* error) const
{
    return fallback_provider_.ensure_directory(directory, error);
}

StorageMutationResult CloudCompatibilityProvider::move_entry(const std::string& source,
                                                             const std::string& destination) const
{
    return fallback_provider_.move_entry(source, destination);
}

StorageMutationResult CloudCompatibilityProvider::undo_move(const std::string& source,
                                                            const std::string& destination) const
{
    return fallback_provider_.undo_move(source, destination);
}
