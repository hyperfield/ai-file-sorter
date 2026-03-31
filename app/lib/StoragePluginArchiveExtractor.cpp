#include "StoragePluginArchiveExtractor.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include <zip.h>

namespace {

std::string ascii_lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool is_directory_entry(const std::string& entry_name)
{
    return !entry_name.empty() && entry_name.back() == '/';
}

std::optional<std::filesystem::path> sanitize_archive_path(const std::string& entry_name)
{
    const std::filesystem::path normalized = std::filesystem::path(entry_name).lexically_normal();
    if (normalized.empty() || normalized.has_root_name() || normalized.has_root_directory()) {
        return std::nullopt;
    }

    std::filesystem::path sanitized;
    for (const auto& component : normalized) {
        if (component == "." || component.empty()) {
            continue;
        }
        if (component == "..") {
            return std::nullopt;
        }
        sanitized /= component;
    }

    if (sanitized.empty()) {
        return std::nullopt;
    }
    return sanitized;
}

std::string archive_error_message(zip_t* archive)
{
    const char* message = zip_strerror(archive);
    return message ? std::string(message) : std::string("Unknown ZIP archive error.");
}

bool apply_zip_permissions(zip_t* archive,
                           zip_uint64_t index,
                           const std::filesystem::path& destination_path)
{
    zip_uint8_t opsys = ZIP_OPSYS_DEFAULT;
    zip_uint32_t attributes = 0;
    if (zip_file_get_external_attributes(archive, index, 0, &opsys, &attributes) != 0) {
        return true;
    }

    if (opsys != ZIP_OPSYS_UNIX) {
        return true;
    }

    const auto mode = static_cast<std::filesystem::perms>((attributes >> 16) & 07777);
    if (mode == std::filesystem::perms::none) {
        return true;
    }

    std::error_code ec;
    std::filesystem::permissions(destination_path, mode, std::filesystem::perm_options::replace, ec);
    return !ec;
}

bool extract_entry(zip_t* archive,
                   zip_uint64_t index,
                   const std::filesystem::path& destination_root,
                   const std::filesystem::path& relative_path,
                   std::string* error)
{
    const auto destination_path = destination_root / relative_path;
    const auto partial_path = std::filesystem::path(destination_path.string() + ".part");

    std::error_code ec;
    std::filesystem::create_directories(destination_path.parent_path(), ec);
    if (ec) {
        if (error) {
            *error = "Failed to create plugin extraction directory: " + ec.message();
        }
        return false;
    }

    zip_file_t* file = zip_fopen_index(archive, index, 0);
    if (!file) {
        if (error) {
            *error = "Failed to open archive entry.";
        }
        return false;
    }

    std::ofstream out(partial_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        zip_fclose(file);
        if (error) {
            *error = "Failed to create extracted plugin asset.";
        }
        return false;
    }

    std::array<char, 64 * 1024> buffer{};
    while (true) {
        const zip_int64_t read = zip_fread(file, buffer.data(), buffer.size());
        if (read < 0) {
            out.close();
            zip_fclose(file);
            std::filesystem::remove(partial_path, ec);
            if (error) {
                *error = archive_error_message(archive);
            }
            return false;
        }
        if (read == 0) {
            break;
        }
        out.write(buffer.data(), static_cast<std::streamsize>(read));
        if (!out) {
            out.close();
            zip_fclose(file);
            std::filesystem::remove(partial_path, ec);
            if (error) {
                *error = "Failed while writing extracted plugin data.";
            }
            return false;
        }
    }

    out.close();
    zip_fclose(file);

    std::filesystem::remove(destination_path, ec);
    ec.clear();
    std::filesystem::rename(partial_path, destination_path, ec);
    if (ec) {
        std::filesystem::remove(partial_path, ec);
        if (error) {
            *error = "Failed to finalize extracted plugin asset: " + ec.message();
        }
        return false;
    }

    if (!apply_zip_permissions(archive, index, destination_path) && error) {
        *error = "Failed to apply archive permissions to extracted plugin asset.";
        return false;
    }

    return true;
}

} // namespace

bool StoragePluginArchiveExtractor::supports_archive(const std::filesystem::path& package_path)
{
    const auto extension = ascii_lower_copy(package_path.extension().string());
    return extension == ".aifsplugin" || extension == ".zip";
}

StoragePluginArchiveExtractor::ExtractionResult StoragePluginArchiveExtractor::extract_archive(
    const std::filesystem::path& archive_path,
    const std::filesystem::path& destination_root)
{
    if (!supports_archive(archive_path)) {
        return ExtractionResult::failure("Only .aifsplugin and .zip plugin packages are supported.");
    }

    int error_code = 0;
    zip_t* archive = zip_open(archive_path.string().c_str(), ZIP_RDONLY, &error_code);
    if (!archive) {
        zip_error_t error;
        zip_error_init_with_code(&error, error_code);
        const char* message = zip_error_strerror(&error);
        const std::string error_message = message ? std::string(message) : std::string("Failed to open plugin archive.");
        zip_error_fini(&error);
        return ExtractionResult::failure(error_message);
    }

    std::optional<std::filesystem::path> manifest_path;
    const zip_int64_t entry_count = zip_get_num_entries(archive, 0);
    if (entry_count < 0) {
        const std::string error_message = archive_error_message(archive);
        zip_close(archive);
        return ExtractionResult::failure(error_message);
    }

    for (zip_uint64_t index = 0; index < static_cast<zip_uint64_t>(entry_count); ++index) {
        const char* raw_name = zip_get_name(archive, index, 0);
        if (!raw_name) {
            continue;
        }

        const std::string entry_name(raw_name);
        const auto sanitized = sanitize_archive_path(entry_name);
        if (!sanitized) {
            zip_close(archive);
            return ExtractionResult::failure("The plugin archive contains an unsafe path.");
        }

        if (is_directory_entry(entry_name)) {
            std::error_code ec;
            std::filesystem::create_directories(destination_root / *sanitized, ec);
            if (ec) {
                zip_close(archive);
                return ExtractionResult::failure("Failed to create extracted plugin directory: " + ec.message());
            }
            continue;
        }

        std::string extraction_error;
        if (!extract_entry(archive, index, destination_root, *sanitized, &extraction_error)) {
            zip_close(archive);
            return ExtractionResult::failure(extraction_error);
        }

        if (sanitized->filename() == "manifest.json") {
            if (manifest_path.has_value()) {
                zip_close(archive);
                return ExtractionResult::failure("The plugin archive contains multiple manifest.json files.");
            }
            manifest_path = destination_root / *sanitized;
        }
    }

    zip_close(archive);

    if (!manifest_path.has_value()) {
        return ExtractionResult::failure("The plugin archive does not contain a manifest.json file.");
    }

    return ExtractionResult::success(destination_root, *manifest_path);
}
