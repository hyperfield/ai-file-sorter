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

    category_path = std::filesystem::path(dir_path) / category;
    subcategory_path = category_path / subcategory;
    destination_path = subcategory_path / file_name;
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
    if (use_subcategory) {
        categorized_path = std::filesystem::path(dir_path) / category / subcategory;
    } else {
        categorized_path = std::filesystem::path(dir_path) / category;
    }
    std::filesystem::path destination_path = categorized_path / file_name;
    std::filesystem::path source_path = std::filesystem::path(dir_path) / file_name;

    if (!std::filesystem::exists(source_path)) {
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->warn("Source file missing when moving '{}': {}", file_name, source_path.string());
        }
        return false;
    }

    if (!std::filesystem::exists(destination_path)) {
        try {
            std::filesystem::rename(source_path, destination_path);
            if (auto logger = Logger::get_logger("core_logger")) {
                logger->info("Moved '{}' to '{}'", source_path.string(), destination_path.string());
            }
            return true;
        } catch (const std::filesystem::filesystem_error& e) {
            if (auto logger = Logger::get_logger("core_logger")) {
                logger->error("Failed to move '{}' to '{}': {}", source_path.string(), destination_path.string(), e.what());
            }
            return false;
        }
    } else {
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->info("Destination already contains '{}'; skipping move", destination_path.string());
        }
        return false;
    }
}


std::string MovableCategorizedFile::get_subcategory_path() const
{
    return subcategory_path.string();
}


std::string MovableCategorizedFile::get_category_path() const
{
    return category_path.string();
}


std::string MovableCategorizedFile::get_destination_path() const
{
    return destination_path.string();
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
