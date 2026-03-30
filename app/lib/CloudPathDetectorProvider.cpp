#include "CloudPathDetectorProvider.hpp"

#include "CloudPathSupport.hpp"

CloudPathDetectorProvider::CloudPathDetectorProvider(std::string detector_id,
                                                     std::string logical_provider_id,
                                                     std::string display_name,
                                                     std::vector<std::string> path_markers,
                                                     std::vector<std::string> env_var_names)
    : detector_id_(std::move(detector_id)),
      logical_provider_id_(std::move(logical_provider_id)),
      display_name_(std::move(display_name)),
      path_markers_(std::move(path_markers)),
      env_var_names_(std::move(env_var_names))
{
}

std::string CloudPathDetectorProvider::id() const
{
    return detector_id_;
}

StorageProviderDetection CloudPathDetectorProvider::detect(const std::string& root_path) const
{
    const CloudPathMatch match = detect_cloud_path_match(root_path, path_markers_, env_var_names_);
    if (!match.matched || match.confidence <= 0) {
        return {};
    }

    return StorageProviderDetection{
        .provider_id = logical_provider_id_,
        .matched = true,
        .needs_additional_support = true,
        .confidence = match.confidence,
        .message = "Detected a " + display_name_ +
            " folder. Dedicated compatibility support is not installed, so the app will continue in local filesystem mode."
    };
}

StorageProviderCapabilities CloudPathDetectorProvider::capabilities() const
{
    return StorageProviderCapabilities{
        .supports_online_only_files = true,
        .supports_atomic_rename = false,
        .should_skip_reparse_points = true,
        .should_relax_undo_mtime_validation = true
    };
}

std::vector<FileEntry> CloudPathDetectorProvider::list_directory(const std::string& directory,
                                                                 FileScanOptions options) const
{
    return fallback_provider_.list_directory(directory, options);
}

bool CloudPathDetectorProvider::path_exists(const std::string& path) const
{
    return fallback_provider_.path_exists(path);
}

bool CloudPathDetectorProvider::ensure_directory(const std::string& directory, std::string* error) const
{
    return fallback_provider_.ensure_directory(directory, error);
}

StorageMutationResult CloudPathDetectorProvider::move_entry(const std::string& source,
                                                            const std::string& destination) const
{
    return fallback_provider_.move_entry(source, destination);
}

StorageMutationResult CloudPathDetectorProvider::undo_move(const std::string& source,
                                                           const std::string& destination) const
{
    return fallback_provider_.undo_move(source, destination);
}
