#include "MovableCategorizedFile.hpp"
#include "Utils.hpp"
#include "Logger.hpp"
#include <filesystem>
#include <cstdio>
#include <algorithm>
#include <cctype>
#include <vector>

namespace {
template <typename Callable>
void with_core_logger(Callable callable)
{
    if (auto logger = Logger::get_logger("core_logger")) {
        callable(*logger);
    }
}

std::string to_lower_copy_str(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool contains_only_allowed_chars(const std::string& value) {
    for (unsigned char ch : value) {
        if (std::iscntrl(ch)) {
            return false;
        }
        static const std::string forbidden = R"(<>:"/\|?*)";
        if (forbidden.find(static_cast<char>(ch)) != std::string::npos) {
            return false;
        }
        // Everything else is allowed (including non-ASCII letters and punctuation).
    }
    return true;
}

bool has_leading_or_trailing_space_or_dot(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    const unsigned char first = static_cast<unsigned char>(value.front());
    const unsigned char last = static_cast<unsigned char>(value.back());
    // Only guard leading/trailing whitespace; dots are allowed.
    return std::isspace(first) || std::isspace(last);
}

bool is_reserved_windows_name(const std::string& value) {
    static const std::vector<std::string> reserved = {
        "con","prn","aux","nul",
        "com1","com2","com3","com4","com5","com6","com7","com8","com9",
        "lpt1","lpt2","lpt3","lpt4","lpt5","lpt6","lpt7","lpt8","lpt9"
    };
    const std::string lower = to_lower_copy_str(value);
    return std::find(reserved.begin(), reserved.end(), lower) != reserved.end();
}

bool looks_like_extension_label(const std::string& value) {
    const auto dot_pos = value.rfind('.');
    if (dot_pos == std::string::npos || dot_pos == value.size() - 1) {
        return false;
    }
    const std::string ext = value.substr(dot_pos + 1);
    if (ext.empty() || ext.size() > 5) {
        return false;
    }
    return std::all_of(ext.begin(), ext.end(), [](unsigned char ch) { return std::isalpha(ch); });
}

bool validate_labels(const std::string& category,
                     const std::string& subcategory,
                     std::string& error) {
    constexpr size_t kMaxLabelLength = 80;
    if (category.empty() || subcategory.empty()) {
        error = "Category or subcategory is empty";
        return false;
    }
    if (category.size() > kMaxLabelLength || subcategory.size() > kMaxLabelLength) {
        error = "Category or subcategory exceeds max length";
        return false;
    }
    if (!contains_only_allowed_chars(category) || !contains_only_allowed_chars(subcategory)) {
        error = "Category or subcategory contains disallowed characters";
        return false;
    }
    if (looks_like_extension_label(category) || looks_like_extension_label(subcategory)) {
        error = "Category or subcategory looks like a file extension";
        return false;
    }
    if (is_reserved_windows_name(category) || is_reserved_windows_name(subcategory)) {
        error = "Category or subcategory is a reserved name";
        return false;
    }
    if (has_leading_or_trailing_space_or_dot(category) || has_leading_or_trailing_space_or_dot(subcategory)) {
        error = "Category or subcategory has leading/trailing space or dot";
        return false;
    }
    return true;
}
}

MovableCategorizedFile::MovePaths
MovableCategorizedFile::build_move_paths(bool use_subcategory) const
{
    const std::filesystem::path base_dir = Utils::utf8_to_path(dir_path);
    const std::filesystem::path category_segment = Utils::utf8_to_path(category);
    const std::filesystem::path subcategory_segment = Utils::utf8_to_path(subcategory);
    const std::filesystem::path file_segment = Utils::utf8_to_path(file_name);
    const std::filesystem::path destination_segment = Utils::utf8_to_path(destination_file_name);

    const std::filesystem::path categorized_root = use_subcategory
        ? base_dir / category_segment / subcategory_segment
        : base_dir / category_segment;

    return MovePaths{
        Utils::utf8_to_path(source_dir) / file_segment,
        categorized_root / destination_segment
    };
}

StorageMutationResult MovableCategorizedFile::perform_move(const std::filesystem::path& source_path,
                                                           const std::filesystem::path& destination_path) const
{
    const auto preflight = storage_provider_.preflight_move(Utils::path_to_utf8(source_path),
                                                            Utils::path_to_utf8(destination_path));
    if (!preflight.allowed) {
        with_core_logger([&](auto& logger) {
            logger.warn("Preflight blocked move '{}' -> '{}': {}",
                        Utils::path_to_utf8(source_path),
                        Utils::path_to_utf8(destination_path),
                        preflight.message);
        });
        return StorageMutationResult{
            .success = false,
            .skipped = preflight.skipped,
            .message = preflight.message,
            .metadata = {
                .size_bytes = 0,
                .mtime = 0,
                .stable_identity = preflight.source_status.stable_identity,
                .revision_token = preflight.source_status.revision_token
            }
        };
    }

    auto result = storage_provider_.move_entry(Utils::path_to_utf8(source_path),
                                               Utils::path_to_utf8(destination_path));
    if (result.success) {
        with_core_logger([&](auto& logger) {
            logger.info("Moved '{}' to '{}'", Utils::path_to_utf8(source_path), Utils::path_to_utf8(destination_path));
        });
    } else if (!result.skipped) {
        with_core_logger([&](auto& logger) {
            logger.error("Failed to move '{}' to '{}': {}",
                         Utils::path_to_utf8(source_path),
                         Utils::path_to_utf8(destination_path),
                         result.message);
        });
    }
    return result;
}


MovableCategorizedFile::MovableCategorizedFile(
    const IStorageProvider& storage_provider,
    const std::string& dir_path, const std::string& cat, const std::string& subcat,
    const std::string& file_name, const std::string& destination_name)
    : MovableCategorizedFile(storage_provider, dir_path, dir_path, cat, subcat, file_name, destination_name)
{
}

MovableCategorizedFile::MovableCategorizedFile(
    const IStorageProvider& storage_provider,
    const std::string& source_dir,
    const std::string& destination_root,
    const std::string& cat,
    const std::string& subcat,
    const std::string& file_name,
    const std::string& destination_name)
    : storage_provider_(storage_provider),
      file_name(file_name),
      destination_file_name(destination_name.empty() ? file_name : destination_name),
      source_dir(source_dir),
      dir_path(destination_root),
      category(cat),
      subcategory(subcat)
{
    std::string validation_error;
    if (!validate_labels(category, subcategory, validation_error)) {
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->error("Invalid path components while constructing MovableCategorizedFile (dir='{}', category='{}', subcategory='{}', file='{}'): {}",
                          dir_path, category, subcategory, file_name, validation_error);
        }
        throw std::runtime_error("Invalid category/subcategory: " + validation_error);
    }
    if (dir_path.empty() || category.empty() || subcategory.empty() || file_name.empty()) {
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->error("Invalid path components while constructing MovableCategorizedFile (dir='{}', category='{}', subcategory='{}', file='{}')",
                          dir_path, category, subcategory, file_name);
        }
        throw std::runtime_error("Invalid path component in CategorizedFile constructor.");
    }

    const std::filesystem::path base_dir = Utils::utf8_to_path(dir_path);
    category_path = base_dir / Utils::utf8_to_path(category);
    subcategory_path = category_path / Utils::utf8_to_path(subcategory);
    destination_path = subcategory_path / Utils::utf8_to_path(destination_file_name);
}


void MovableCategorizedFile::create_cat_dirs(bool use_subcategory)
{
    std::string error;
    if (!storage_provider_.ensure_directory(Utils::path_to_utf8(category_path), &error)) {
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->error("Failed to create category directory for '{}': {}", file_name, error);
        }
        throw std::runtime_error(error.empty() ? "Failed to create category directory." : error);
    }
    if (use_subcategory &&
        !storage_provider_.ensure_directory(Utils::path_to_utf8(subcategory_path), &error)) {
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->error("Failed to create subcategory directory for '{}': {}", file_name, error);
        }
        throw std::runtime_error(error.empty() ? "Failed to create subcategory directory." : error);
    }
}


StorageMutationResult MovableCategorizedFile::move_file(bool use_subcategory)
{
    const MovePaths paths = build_move_paths(use_subcategory);

    return perform_move(paths.source, paths.destination);
}

MovableCategorizedFile::PreviewPaths
MovableCategorizedFile::preview_move_paths(bool use_subcategory) const
{
    const MovePaths paths = build_move_paths(use_subcategory);
    return PreviewPaths{
        Utils::path_to_utf8(paths.source),
        Utils::path_to_utf8(paths.destination)
    };
}


std::string MovableCategorizedFile::get_subcategory_path() const
{
    return Utils::path_to_utf8(subcategory_path);
}


std::string MovableCategorizedFile::get_category_path() const
{
    return Utils::path_to_utf8(category_path);
}


std::string MovableCategorizedFile::get_destination_path() const
{
    return Utils::path_to_utf8(destination_path);
}


std::string MovableCategorizedFile::get_file_name() const
{
    return file_name;
}

std::string MovableCategorizedFile::get_dir_path() const
{
    return dir_path;
}

std::string MovableCategorizedFile::get_category() const
{
    return category;
}

std::string MovableCategorizedFile::get_subcategory() const
{
    return subcategory;
}

void MovableCategorizedFile::set_category(std::string& category)
{
    this->category = category;
}

void MovableCategorizedFile::set_subcategory(std::string& subcategory)
{
    this->subcategory = subcategory;
}

MovableCategorizedFile::~MovableCategorizedFile() {}
