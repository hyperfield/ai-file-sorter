#include "OneDriveMacRootDetection.hpp"

#include <QByteArrayView>

#include <sys/stat.h>
#include <sys/xattr.h>

#include <array>
#include <cerrno>
#include <utility>

namespace one_drive {
namespace {

// Microsoft documents this macOS attribute and the standalone and App Store
// provider IDs in “Why do I have two versions of OneDrive on my Mac?”
constexpr auto kDomainAttribute = "com.apple.file-provider-domain-id";
constexpr QByteArrayView kStandaloneProvider = "com.microsoft.OneDrive.FileProvider";
constexpr QByteArrayView kAppStoreProvider = "com.microsoft.OneDrive-mac.FileProvider";
constexpr std::size_t kMaxDomainAttributeBytes = 1024;

std::optional<QByteArray> read_file_provider_domain(
    const std::filesystem::path &directory, std::error_code &error) {
  std::array<char, kMaxDomainAttributeBytes> value;
  const auto size = getxattr(directory.c_str(), kDomainAttribute, value.data(),
                             value.size(), 0, XATTR_NOFOLLOW);
  if (size >= 0) {
    return QByteArray(value.data(), static_cast<qsizetype>(size));
  }

  const int read_error = errno;
  if (read_error != ENOATTR) {
    error = std::error_code(read_error, std::generic_category());
  }
  return std::nullopt;
}

SyncRootDetection classify_file_provider_domain(QByteArrayView domain) {
  const auto separator = domain.indexOf('/');
  if (separator <= 0 || separator + 1 == domain.size() || domain.contains('\0')) {
    return {};
  }

  const auto provider = domain.first(separator);
  const bool is_onedrive =
      provider == kStandaloneProvider || provider == kAppStoreProvider;
  return SyncRootDetection{
      .matched = is_onedrive,
      .authoritative = true,
      .vetoed = !is_onedrive,
      .confidence = is_onedrive ? 95 : 0,
      .source = QStringLiteral("macos_file_provider"),
      .provider_name = is_onedrive ? QStringLiteral("Microsoft OneDrive") : QString(),
      .message = is_onedrive
                     ? QStringLiteral("Detected OneDrive via macOS File Provider.")
                     : QStringLiteral("A different macOS File Provider manages this folder.")};
}

} // namespace

SyncRootDetection detect_macos_onedrive_root(const std::filesystem::path &path) {
  return detect_macos_onedrive_root(path, read_file_provider_domain);
}

SyncRootDetection detect_macos_onedrive_root(
    const std::filesystem::path &path, const FileProviderDomainReader &read_domain) {
  if (path.empty() || path.native().find('\0') != std::filesystem::path::string_type::npos) {
    return {};
  }

  std::error_code error;
  auto directory = std::filesystem::canonical(path, error);
  if (error) {
    return {};
  }

  // Find the nearest registered ancestor without inheriting a domain from a
  // different volume. Root metadata also works for relocated OneDrive domains.
  std::optional<dev_t> device;
  while (true) {
    struct stat info {};
    if (lstat(directory.c_str(), &info) != 0 || !S_ISDIR(info.st_mode) ||
        (device && info.st_dev != *device)) {
      return {};
    }
    device = info.st_dev;

    const auto domain = read_domain(directory, error);
    if (error) {
      return {};
    }
    if (domain) {
      return classify_file_provider_domain(*domain);
    }

    auto parent = directory.parent_path();
    if (parent == directory) {
      return {};
    }
    directory = std::move(parent);
  }
}

} // namespace one_drive
