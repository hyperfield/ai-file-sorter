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
        base_dir / file_segment,
        categorized_root / destination_segment
    };
}

bool MovableCategorizedFile::source_is_available(const std::filesystem::path& source_path) const
{
    if (std::filesystem::exists(source_path)) {
        return true;
    }

    with_core_logger([&](auto& logger) {
        logger.warn("Source file missing when moving '{}': {}", file_name, Utils::path_to_utf8(source_path));
    });
    return false;
}

bool MovableCategorizedFile::destination_is_available(const std::filesystem::path& destination_path) const
{
    if (!std::filesystem::exists(destination_path)) {
        return true;
    }

    with_core_logger([&](auto& logger) {
        logger.info("Destination already contains '{}'; skipping move", Utils::path_to_utf8(destination_path));
    });
    return false;
}

bool MovableCategorizedFile::perform_move(const std::filesystem::path& source_path,
                                          const std::filesystem::path& destination_path) const
{
    try {
        std::filesystem::rename(source_path, destination_path);
        with_core_logger([&](auto& logger) {
            logger.info("Moved '{}' to '{}'", Utils::path_to_utf8(source_path), Utils::path_to_utf8(destination_path));
        });
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        with_core_logger([&](auto& logger) {
            logger.error("Failed to move '{}' to '{}': {}", Utils::path_to_utf8(source_path), Utils::path_to_utf8(destination_path), e.what());
        });
        return false;
    }
}


MovableCategorizedFile::MovableCategorizedFile(
    const std::string& dir_path, const std::string& cat, const std::string& subcat,
    const std::string& file_name, const std::string& destination_name)
    : file_name(file_name),
      destination_file_name(destination_name.empty() ? file_name : destination_name),
      dir_path(dir_path),
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
    try {
        if (!std::filesystem::exists(category_path)) {
            std::filesystem::create_directory(category_path);
        }
        if (use_subcategory && !std::filesystem::exists(subcategory_path)) {
            std::filesystem::create_directory(subcategory_path);
        }
    } catch (const std::filesystem::filesystem_error& e) {
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->error("Failed to create directories for '{}': {}", file_name, e.what());
        }
        throw;
    }
}


bool MovableCategorizedFile::move_file(bool use_subcategory)
{
    const MovePaths paths = build_move_paths(use_subcategory);

    if (!source_is_available(paths.source)) {
        return false;
    }

    if (!destination_is_available(paths.destination)) {
        return false;
    }

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
