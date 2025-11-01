#include "MovableCategorizedFile.hpp"
#include "Utils.hpp"
#include "Logger.hpp"
#include <filesystem>
#include <cstdio>


MovableCategorizedFile::MovableCategorizedFile(
    const std::string& dir_path, const std::string& cat, const std::string& subcat,
    const std::string& file_name, const std::string& file_type)
    : file_name(file_name),
      file_type(file_type),
      dir_path(dir_path),
      category(cat),
      subcategory(subcat)
{
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
    destination_path = subcategory_path / Utils::utf8_to_path(file_name);
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
    std::filesystem::path categorized_path;
    const std::filesystem::path base_dir = Utils::utf8_to_path(dir_path);
    const std::filesystem::path category_segment = Utils::utf8_to_path(category);
    const std::filesystem::path subcategory_segment = Utils::utf8_to_path(subcategory);
    const std::filesystem::path file_segment = Utils::utf8_to_path(file_name);

    if (use_subcategory) {
        categorized_path = base_dir / category_segment / subcategory_segment;
    } else {
        categorized_path = base_dir / category_segment;
    }
    std::filesystem::path destination_path = categorized_path / file_segment;
    std::filesystem::path source_path = base_dir / file_segment;

    if (!std::filesystem::exists(source_path)) {
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->warn("Source file missing when moving '{}': {}", file_name, Utils::path_to_utf8(source_path));
        }
        return false;
    }

    if (!std::filesystem::exists(destination_path)) {
        try {
            std::filesystem::rename(source_path, destination_path);
            if (auto logger = Logger::get_logger("core_logger")) {
                logger->info("Moved '{}' to '{}'", Utils::path_to_utf8(source_path), Utils::path_to_utf8(destination_path));
            }
            return true;
        } catch (const std::filesystem::filesystem_error& e) {
            if (auto logger = Logger::get_logger("core_logger")) {
                logger->error("Failed to move '{}' to '{}': {}", Utils::path_to_utf8(source_path), Utils::path_to_utf8(destination_path), e.what());
            }
            return false;
        }
    } else {
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->info("Destination already contains '{}'; skipping move", Utils::path_to_utf8(destination_path));
        }
        return false;
    }
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
