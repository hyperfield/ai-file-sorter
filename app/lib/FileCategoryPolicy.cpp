#include "TextEncoding.hpp"
#include "FileCategoryPolicy.hpp"

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <string>
#include <string_view>
#include <unordered_set>

namespace {

enum class FamilyKind {
    Generic,
    Image,
    Document,
    Software,
    Archive,
    Audio,
    Video,
    Ebook,
    Font
};

std::string to_lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(TextEncoding::ascii_lower(ch));
    });
    return value;
}

std::string normalize_file_name(std::string_view file_name)
{
    const auto newline = file_name.find('\n');
    return to_lower_copy(std::string(file_name.substr(0, newline)));
}

std::string extract_extension_lower(std::string_view file_name)
{
    const std::string base_name = normalize_file_name(file_name);
    const auto dot = base_name.find_last_of('.');
    if (dot == std::string::npos || dot + 1 >= base_name.size()) {
        return {};
    }
    return base_name.substr(dot);
}

bool ends_with(std::string_view value, std::string_view suffix)
{
    return value.size() >= suffix.size() &&
           value.substr(value.size() - suffix.size()) == suffix;
}

bool matches_any_suffix(std::string_view value, std::initializer_list<std::string_view> suffixes)
{
    return std::any_of(suffixes.begin(), suffixes.end(), [&](std::string_view suffix) {
        return ends_with(value, suffix);
    });
}

const std::vector<std::string>& document_categories()
{
    static const std::vector<std::string> categories = {
        "Documents", "Presentations", "Spreadsheets", "Data Exports", "Configs"
    };
    return categories;
}

const std::vector<std::string>& image_categories()
{
    static const std::vector<std::string> categories = {"Images"};
    return categories;
}

const std::vector<std::string>& software_categories()
{
    static const std::vector<std::string> categories = {
        "Software", "Installers", "Drivers", "Operating Systems", "Other"
    };
    return categories;
}

const std::vector<std::string>& archive_categories()
{
    static const std::vector<std::string> categories = {
        "Archives", "Software", "Data Exports", "Other"
    };
    return categories;
}

const std::vector<std::string>& audio_categories()
{
    static const std::vector<std::string> categories = {"Audio", "Other"};
    return categories;
}

const std::vector<std::string>& video_categories()
{
    static const std::vector<std::string> categories = {"Videos", "Other"};
    return categories;
}

const std::vector<std::string>& ebook_categories()
{
    static const std::vector<std::string> categories = {"Ebooks", "Documents", "Other"};
    return categories;
}

const std::vector<std::string>& font_categories()
{
    static const std::vector<std::string> categories = {"Fonts", "Other"};
    return categories;
}

const std::vector<std::string>& generic_categories()
{
    static const std::vector<std::string> categories = {
        "Documents", "Images", "Videos", "Audio", "Software", "Archives",
        "Data Exports", "Configs", "Drivers", "Operating Systems", "Ebooks",
        "Fonts", "Other"
    };
    return categories;
}

const std::unordered_set<std::string> kCategorizedImageExtensions = {
    ".jpg", ".jpeg", ".png", ".bmp", ".gif", ".webp", ".tif", ".tiff",
    ".tga", ".psd", ".hdr", ".pic", ".pnm", ".ppm", ".pgm", ".pbm",
    ".heic", ".heif", ".avif", ".ico", ".svg"
};

const std::unordered_set<std::string> kCategorizedDocumentExtensions = {
    ".txt", ".md", ".markdown", ".rtf", ".csv", ".tsv", ".log", ".json", ".xml", ".yml", ".yaml",
    ".ini", ".cfg", ".conf", ".html", ".htm", ".tex", ".rst", ".pdf", ".docx", ".xlsx", ".pptx",
    ".odt", ".ods", ".odp", ".doc", ".xls", ".ppt"
};

const std::unordered_set<std::string> kPresentationExtensions = {
    ".pptx", ".odp", ".ppt"
};

const std::unordered_set<std::string> kSpreadsheetExtensions = {
    ".xlsx", ".ods", ".xls"
};

const std::unordered_set<std::string> kDataExportExtensions = {
    ".csv", ".tsv"
};

const std::unordered_set<std::string> kConfigExtensions = {
    ".ini", ".cfg", ".conf"
};

bool is_software_artifact_file_name(const std::string& file_name)
{
    return matches_any_suffix(file_name,
                              {".exe", ".msi", ".msix", ".msixbundle", ".appx",
                               ".appxbundle", ".deb", ".rpm", ".pkg", ".dmg",
                               ".appimage", ".apk", ".run", ".bat", ".cmd", ".com"});
}

bool is_archive_file_name(const std::string& file_name)
{
    return matches_any_suffix(file_name,
                              {".zip", ".7z", ".rar", ".tar", ".gz", ".bz2", ".xz",
                               ".tgz", ".tbz", ".tbz2", ".txz", ".tar.gz",
                               ".tar.bz2", ".tar.xz"});
}

bool is_audio_file_name(const std::string& file_name)
{
    return matches_any_suffix(file_name,
                              {".aac", ".aif", ".aiff", ".alac", ".ape", ".flac",
                               ".m4a", ".mp3", ".ogg", ".oga", ".opus", ".wav",
                               ".wma"});
}

bool is_video_file_name(const std::string& file_name)
{
    return matches_any_suffix(file_name,
                              {".3gp", ".avi", ".flv", ".m4v", ".mkv", ".mov",
                               ".mp4", ".mpeg", ".mpg", ".mts", ".m2ts", ".ts",
                               ".webm", ".wmv"});
}

bool is_ebook_file_name(const std::string& file_name)
{
    return matches_any_suffix(file_name, {".epub", ".mobi", ".azw", ".azw3", ".fb2"});
}

bool is_font_file_name(const std::string& file_name)
{
    return matches_any_suffix(file_name, {".ttf", ".otf", ".woff", ".woff2"});
}

bool contains_extension(const std::unordered_set<std::string>& extensions,
                        const std::string& extension)
{
    return !extension.empty() && extensions.contains(extension);
}

std::optional<std::string> preferred_document_main_category_for_extension(const std::string& extension)
{
    if (!contains_extension(kCategorizedDocumentExtensions, extension)) {
        return std::nullopt;
    }
    if (contains_extension(kPresentationExtensions, extension)) {
        return std::string("Presentations");
    }
    if (contains_extension(kSpreadsheetExtensions, extension)) {
        return std::string("Spreadsheets");
    }
    if (contains_extension(kDataExportExtensions, extension)) {
        return std::string("Data Exports");
    }
    if (contains_extension(kConfigExtensions, extension)) {
        return std::string("Configs");
    }
    return std::string("Documents");
}

FamilyKind determine_file_family_kind(const std::string& file_name)
{
    const std::string extension = extract_extension_lower(file_name);
    if (contains_extension(kCategorizedImageExtensions, extension)) {
        return FamilyKind::Image;
    }
    if (preferred_document_main_category_for_extension(extension).has_value()) {
        return FamilyKind::Document;
    }

    const std::string normalized_file_name = normalize_file_name(file_name);
    if (normalized_file_name.empty()) {
        return FamilyKind::Generic;
    }
    if (is_software_artifact_file_name(normalized_file_name)) {
        return FamilyKind::Software;
    }
    if (is_archive_file_name(normalized_file_name)) {
        return FamilyKind::Archive;
    }
    if (is_video_file_name(normalized_file_name)) {
        return FamilyKind::Video;
    }
    if (is_audio_file_name(normalized_file_name)) {
        return FamilyKind::Audio;
    }
    if (is_ebook_file_name(normalized_file_name)) {
        return FamilyKind::Ebook;
    }
    if (is_font_file_name(normalized_file_name)) {
        return FamilyKind::Font;
    }
    return FamilyKind::Generic;
}

FileCategoryPolicy::MainCategorySelection make_selection(FamilyKind family_kind,
                                                         const std::vector<std::string>& categories)
{
    std::string family_name;
    switch (family_kind) {
        case FamilyKind::Image:
            family_name = "image";
            break;
        case FamilyKind::Document:
            family_name = "document";
            break;
        case FamilyKind::Software:
            family_name = "software";
            break;
        case FamilyKind::Archive:
            family_name = "archive";
            break;
        case FamilyKind::Audio:
            family_name = "audio";
            break;
        case FamilyKind::Video:
            family_name = "video";
            break;
        case FamilyKind::Ebook:
            family_name = "ebook";
            break;
        case FamilyKind::Font:
            family_name = "font";
            break;
        case FamilyKind::Generic:
        default:
            family_name = "generic";
            break;
    }
    return FileCategoryPolicy::MainCategorySelection{std::move(family_name), categories};
}

} // namespace

namespace FileCategoryPolicy {

MainCategorySelection determine_main_category_selection(const std::string& file_name,
                                                        FileType file_type)
{
    if (file_type != FileType::File) {
        return {};
    }

    switch (determine_file_family_kind(file_name)) {
        case FamilyKind::Image:
            return make_selection(FamilyKind::Image, image_categories());
        case FamilyKind::Document:
            return make_selection(FamilyKind::Document, document_categories());
        case FamilyKind::Software:
            return make_selection(FamilyKind::Software, software_categories());
        case FamilyKind::Archive:
            return make_selection(FamilyKind::Archive, archive_categories());
        case FamilyKind::Audio:
            return make_selection(FamilyKind::Audio, audio_categories());
        case FamilyKind::Video:
            return make_selection(FamilyKind::Video, video_categories());
        case FamilyKind::Ebook:
            return make_selection(FamilyKind::Ebook, ebook_categories());
        case FamilyKind::Font:
            return make_selection(FamilyKind::Font, font_categories());
        case FamilyKind::Generic:
        default:
            return make_selection(FamilyKind::Generic, generic_categories());
    }
}

bool is_supported_image_file_name(const std::string& file_name)
{
    return determine_file_family_kind(file_name) == FamilyKind::Image;
}

bool is_supported_document_file_name(const std::string& file_name)
{
    return determine_file_family_kind(file_name) == FamilyKind::Document;
}

std::optional<std::string> preferred_main_category_for_file_name(const std::string& file_name)
{
    if (determine_file_family_kind(file_name) == FamilyKind::Image) {
        return std::string("Images");
    }
    return preferred_document_main_category_for_extension(extract_extension_lower(file_name));
}

} // namespace FileCategoryPolicy
