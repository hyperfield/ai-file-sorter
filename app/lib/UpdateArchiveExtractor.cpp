#include "UpdateArchiveExtractor.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

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

bool is_installer_entry(const std::string& entry_name)
{
    const std::string extension = ascii_lower_copy(std::filesystem::path(entry_name).extension().string());
    return extension == ".exe" || extension == ".msi";
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

UpdateArchiveExtractor::ExtractionResult extract_entry(zip_t* archive,
                                                       zip_uint64_t index,
                                                       const std::filesystem::path& destination_root,
                                                       const std::filesystem::path& relative_path)
{
    const auto destination_path = destination_root / relative_path;
    const auto partial_path = std::filesystem::path(destination_path.string() + ".part");

    std::error_code ec;
    std::filesystem::create_directories(destination_path.parent_path(), ec);
    if (ec) {
        return UpdateArchiveExtractor::ExtractionResult::failure(
            "Failed to create archive extraction directory: " + ec.message());
    }

    zip_file_t* file = zip_fopen_index(archive, index, 0);
    if (!file) {
        return UpdateArchiveExtractor::ExtractionResult::failure(
            "Failed to open installer inside the ZIP archive.");
    }

    std::ofstream out(partial_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        zip_fclose(file);
        return UpdateArchiveExtractor::ExtractionResult::failure(
            "Failed to create the extracted installer file.");
    }

    std::array<char, 64 * 1024> buffer{};
    while (true) {
        const zip_int64_t read = zip_fread(file, buffer.data(), buffer.size());
        if (read < 0) {
            out.close();
            zip_fclose(file);
            std::filesystem::remove(partial_path, ec);
            return UpdateArchiveExtractor::ExtractionResult::failure(archive_error_message(archive));
        }
        if (read == 0) {
            break;
        }
        out.write(buffer.data(), static_cast<std::streamsize>(read));
        if (!out) {
            out.close();
            zip_fclose(file);
            std::filesystem::remove(partial_path, ec);
            return UpdateArchiveExtractor::ExtractionResult::failure(
                "Failed while writing the extracted installer.");
        }
    }

    out.close();
    zip_fclose(file);

    std::filesystem::remove(destination_path, ec);
    ec.clear();
    std::filesystem::rename(partial_path, destination_path, ec);
    if (ec) {
        std::filesystem::remove(partial_path, ec);
        return UpdateArchiveExtractor::ExtractionResult::failure(
            "Failed to finalize the extracted installer: " + ec.message());
    }

    return UpdateArchiveExtractor::ExtractionResult::success(destination_path);
}

} // namespace

bool UpdateArchiveExtractor::supports_archive(const std::filesystem::path& package_path)
{
    return ascii_lower_copy(package_path.extension().string()) == ".zip";
}

UpdateArchiveExtractor::ExtractionResult UpdateArchiveExtractor::extract_installer(
    const std::filesystem::path& archive_path,
    const std::filesystem::path& destination_root)
{
    if (!supports_archive(archive_path)) {
        return ExtractionResult::failure("Only ZIP update packages are supported.");
    }

    int error_code = 0;
    zip_t* archive = zip_open(archive_path.string().c_str(), ZIP_RDONLY, &error_code);
    if (!archive) {
        zip_error_t error;
        zip_error_init_with_code(&error, error_code);
        const char* message = zip_error_strerror(&error);
        const std::string error_message = message ? std::string(message) : std::string("Failed to open ZIP archive.");
        zip_error_fini(&error);
        return ExtractionResult::failure(error_message);
    }

    std::vector<std::pair<zip_uint64_t, std::filesystem::path>> installer_candidates;
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
        if (is_directory_entry(entry_name) || !is_installer_entry(entry_name)) {
            continue;
        }

        const auto sanitized = sanitize_archive_path(entry_name);
        if (!sanitized) {
            zip_close(archive);
            return ExtractionResult::failure("The ZIP archive contains an unsafe installer path.");
        }

        installer_candidates.emplace_back(index, *sanitized);
    }

    if (installer_candidates.empty()) {
        zip_close(archive);
        return ExtractionResult::failure(
            "The ZIP archive does not contain an installer (.exe or .msi).");
    }

    if (installer_candidates.size() > 1) {
        zip_close(archive);
        return ExtractionResult::failure(
            "The ZIP archive contains multiple installer candidates. Keep only one .exe or .msi file in the package.");
    }

    const auto [index, relative_path] = installer_candidates.front();
    auto result = extract_entry(archive, index, destination_root, relative_path);
    zip_close(archive);
    return result;
}
