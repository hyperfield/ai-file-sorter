#pragma once

#include "OneDrivePlatform.hpp"

#include <QByteArray>

#include <functional>
#include <optional>
#include <system_error>

namespace one_drive {

// An empty optional without an error means the attribute is absent.
using FileProviderDomainReader = std::function<std::optional<QByteArray>(
    const std::filesystem::path &, std::error_code &)>;

SyncRootDetection detect_macos_onedrive_root(const std::filesystem::path &path);
SyncRootDetection detect_macos_onedrive_root(
    const std::filesystem::path &path, const FileProviderDomainReader &read_domain);

} // namespace one_drive
