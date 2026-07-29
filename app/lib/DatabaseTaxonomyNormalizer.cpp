#include "DatabaseTaxonomyNormalizer.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace DatabaseTaxonomyNormalizer {
namespace {

constexpr char kLegacyMusicCategoryNormalized[] = "music";
constexpr char kCanonicalAudioCategoryNormalized[] = "audio";
constexpr char kCanonicalAudioCategoryDisplay[] = "Audio";
constexpr char kLegacyInstallerBuildersNormalized[] = "installer builders";
constexpr char kCanonicalInstallerToolsNormalized[] = "installer tools";
constexpr char kCanonicalInstallerToolsDisplay[] = "Installer Tools";
constexpr char kArchivesCategoryNormalized[] = "archives";
constexpr char kArchivesCategoryDisplay[] = "Archives";
constexpr char kDataExportsCategoryNormalized[] = "data exports";
constexpr char kDataExportsCategoryDisplay[] = "Data Exports";
constexpr char kDocumentsCategoryNormalized[] = "documents";
constexpr char kDocumentsCategoryDisplay[] = "Documents";
constexpr char kDriversCategoryNormalized[] = "drivers";
constexpr char kDriversCategoryDisplay[] = "Drivers";
constexpr char kFontsCategoryNormalized[] = "fonts";
constexpr char kFontsCategoryDisplay[] = "Fonts";
constexpr char kImagesCategoryNormalized[] = "images";
constexpr char kImagesCategoryDisplay[] = "Images";
constexpr char kInstallersCategoryNormalized[] = "installers";
constexpr char kInstallersCategoryDisplay[] = "Installers";
constexpr char kOperatingSystemsCategoryNormalized[] = "operating systems";
constexpr char kOperatingSystemsCategoryDisplay[] = "Operating Systems";
constexpr char kPresentationsCategoryNormalized[] = "presentations";
constexpr char kPresentationsCategoryDisplay[] = "Presentations";
constexpr char kSoftwareCategoryNormalized[] = "software";
constexpr char kSoftwareCategoryDisplay[] = "Software";
constexpr char kSpreadsheetsCategoryNormalized[] = "spreadsheets";
constexpr char kSpreadsheetsCategoryDisplay[] = "Spreadsheets";
constexpr char kVideosCategoryNormalized[] = "videos";
constexpr char kVideosCategoryDisplay[] = "Videos";

struct CanonicalCategoryLabel {
    std::string normalized;
    std::string display;
};

struct SemanticFamily {
    std::string canonical_category_normalized;
    std::string canonical_category_display;
    std::string parent_category_normalized;
    std::vector<std::string_view> aliases;
    std::vector<std::string_view> parent_generic_aliases;
};

std::string trim_copy(std::string value)
{
    auto is_space = [](unsigned char ch) { return std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(),
                                            [&](unsigned char ch) { return !is_space(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(),
                             [&](unsigned char ch) { return !is_space(ch); }).base(),
                value.end());
    return value;
}

std::optional<CanonicalCategoryLabel> canonicalize_broad_main_label(const std::string& normalized_label)
{
    static const std::unordered_map<std::string, CanonicalCategoryLabel> kBroadMainLabels = {
        {"archive", {kArchivesCategoryNormalized, kArchivesCategoryDisplay}},
        {"archives", {kArchivesCategoryNormalized, kArchivesCategoryDisplay}},
        {"audio", {kCanonicalAudioCategoryNormalized, kCanonicalAudioCategoryDisplay}},
        {"audio file", {kCanonicalAudioCategoryNormalized, kCanonicalAudioCategoryDisplay}},
        {"audio files", {kCanonicalAudioCategoryNormalized, kCanonicalAudioCategoryDisplay}},
        {"config", {"configs", "Configs"}},
        {"configs", {"configs", "Configs"}},
        {"configuration", {"configs", "Configs"}},
        {"configurations", {"configs", "Configs"}},
        {"data export", {kDataExportsCategoryNormalized, kDataExportsCategoryDisplay}},
        {"data exports", {kDataExportsCategoryNormalized, kDataExportsCategoryDisplay}},
        {"document", {kDocumentsCategoryNormalized, kDocumentsCategoryDisplay}},
        {"documents", {kDocumentsCategoryNormalized, kDocumentsCategoryDisplay}},
        {"doc", {kDocumentsCategoryNormalized, kDocumentsCategoryDisplay}},
        {"docs", {kDocumentsCategoryNormalized, kDocumentsCategoryDisplay}},
        {"driver", {kDriversCategoryNormalized, kDriversCategoryDisplay}},
        {"drivers", {kDriversCategoryNormalized, kDriversCategoryDisplay}},
        {"ebook", {"ebooks", "Ebooks"}},
        {"ebooks", {"ebooks", "Ebooks"}},
        {"font", {kFontsCategoryNormalized, kFontsCategoryDisplay}},
        {"fonts", {kFontsCategoryNormalized, kFontsCategoryDisplay}},
        {"image", {kImagesCategoryNormalized, kImagesCategoryDisplay}},
        {"images", {kImagesCategoryNormalized, kImagesCategoryDisplay}},
        {"installer", {kInstallersCategoryNormalized, kInstallersCategoryDisplay}},
        {"installers", {kInstallersCategoryNormalized, kInstallersCategoryDisplay}},
        {"installation", {kInstallersCategoryNormalized, kInstallersCategoryDisplay}},
        {"installations", {kInstallersCategoryNormalized, kInstallersCategoryDisplay}},
        {"operating system", {kOperatingSystemsCategoryNormalized, kOperatingSystemsCategoryDisplay}},
        {"operating systems", {kOperatingSystemsCategoryNormalized, kOperatingSystemsCategoryDisplay}},
        {"presentation", {kPresentationsCategoryNormalized, kPresentationsCategoryDisplay}},
        {"presentations", {kPresentationsCategoryNormalized, kPresentationsCategoryDisplay}},
        {"software", {kSoftwareCategoryNormalized, kSoftwareCategoryDisplay}},
        {"spreadsheet", {kSpreadsheetsCategoryNormalized, kSpreadsheetsCategoryDisplay}},
        {"spreadsheets", {kSpreadsheetsCategoryNormalized, kSpreadsheetsCategoryDisplay}},
        {"video", {kVideosCategoryNormalized, kVideosCategoryDisplay}},
        {"videos", {kVideosCategoryNormalized, kVideosCategoryDisplay}}
    };

    if (auto it = kBroadMainLabels.find(normalized_label); it != kBroadMainLabels.end()) {
        return it->second;
    }

    const std::string stripped_label = strip_trailing_stopwords(normalized_label);
    if (auto it = kBroadMainLabels.find(stripped_label); it != kBroadMainLabels.end()) {
        return it->second;
    }

    return std::nullopt;
}

bool is_generic_family_subcategory(const std::string& normalized_subcategory,
                                   const std::string& normalized_category,
                                   const SemanticFamily& family)
{
    if (normalized_subcategory.empty()) {
        return true;
    }

    static const std::unordered_set<std::string> kGeneric = {
        "general",
        "misc",
        "miscellaneous",
        "other",
        "others",
        "uncategorized",
        "document",
        "documents",
        "doc",
        "docs"
    };

    if (kGeneric.contains(normalized_subcategory)) {
        return true;
    }

    const std::string stripped_subcategory = strip_trailing_stopwords(normalized_subcategory);
    if (kGeneric.contains(stripped_subcategory)) {
        return true;
    }

    if (std::any_of(family.aliases.begin(),
                    family.aliases.end(),
                    [&](std::string_view alias) {
                        return normalized_subcategory == alias || stripped_subcategory == alias;
                    })) {
        return true;
    }

    if (stripped_subcategory == strip_trailing_stopwords(normalized_category) ||
        stripped_subcategory == family.canonical_category_normalized ||
        stripped_subcategory == strip_trailing_stopwords(family.parent_category_normalized)) {
        return true;
    }

    return std::any_of(family.parent_generic_aliases.begin(),
                       family.parent_generic_aliases.end(),
                       [&](std::string_view alias) { return stripped_subcategory == alias; });
}

const std::vector<SemanticFamily>& semantic_families()
{
    static const std::vector<SemanticFamily> kFamilies = {
        {
            "backups", "Backups", "archives",
            {"backup", "backups", "backup file", "backup files"},
            {"archive", "archives"}
        },
        {
            "ebooks", "Ebooks", "books",
            {"ebook", "ebooks", "e book", "e books"},
            {"book", "books"}
        },
        {
            "guides", "Guides", "documents",
            {"guide", "guides"},
            {"document", "documents", "doc", "docs", "text", "texts", "paper", "papers", "office file", "office files"}
        },
        {
            "licenses", "Licenses", "documents",
            {"license", "licenses", "licence", "licences"},
            {"document", "documents", "doc", "docs", "text", "texts", "paper", "papers", "office file", "office files"}
        },
        {
            "manuals", "Manuals", "documents",
            {"manual", "manuals"},
            {"document", "documents", "doc", "docs", "text", "texts", "paper", "papers", "office file", "office files"}
        },
        {
            "presentations", "Presentations", "documents",
            {"presentation", "presentations", "slide deck", "slide decks", "deck", "decks", "slides"},
            {"document", "documents", "doc", "docs", "text", "texts", "paper", "papers", "office file", "office files"}
        },
        {
            "spreadsheets", "Spreadsheets", "documents",
            {"spreadsheet", "spreadsheets", "worksheet", "worksheets"},
            {"document", "documents", "doc", "docs", "text", "texts", "paper", "papers", "office file", "office files", "table", "tables"}
        }
    };
    return kFamilies;
}

bool semantic_family_matches_alias(const SemanticFamily& family, const std::string& normalized_label)
{
    if (normalized_label.empty()) {
        return false;
    }

    const std::string stripped = strip_trailing_stopwords(normalized_label);
    return std::any_of(family.aliases.begin(),
                       family.aliases.end(),
                       [&](std::string_view alias) {
                           return normalized_label == alias || stripped == alias;
                       });
}

const SemanticFamily* find_semantic_family(const std::string& normalized_label)
{
    for (const auto& family : semantic_families()) {
        if (semantic_family_matches_alias(family, normalized_label)) {
            return &family;
        }
    }
    return nullptr;
}

bool is_image_like_label(const std::string& normalized)
{
    if (normalized.empty()) {
        return false;
    }
    static const std::unordered_set<std::string> kImageLike = {
        "image", "images",
        "image file", "image files",
        "photo", "photos",
        "graphic", "graphics",
        "picture", "pictures",
        "pic", "pics",
        "screenshot", "screenshots",
        "wallpaper", "wallpapers"
    };
    if (kImageLike.contains(normalized)) {
        return true;
    }
    return kImageLike.contains(strip_trailing_stopwords(normalized));
}

CanonicalCategoryLabel canonicalize_category_label(const std::string& normalized_category,
                                                   const std::string& normalized_subcategory)
{
    if (const auto broad_main = canonicalize_broad_main_label(normalized_category)) {
        return *broad_main;
    }

    static const std::unordered_map<std::string, CanonicalCategoryLabel> kCategorySynonyms = {
        {"audio", {kCanonicalAudioCategoryNormalized, kCanonicalAudioCategoryDisplay}},
        {"audio file", {kCanonicalAudioCategoryNormalized, kCanonicalAudioCategoryDisplay}},
        {"audio files", {kCanonicalAudioCategoryNormalized, kCanonicalAudioCategoryDisplay}},
        {kLegacyMusicCategoryNormalized, {kCanonicalAudioCategoryNormalized, kCanonicalAudioCategoryDisplay}},

        {"document", {"documents", "Documents"}},
        {"documents", {"documents", "Documents"}},
        {"doc", {"documents", "Documents"}},
        {"docs", {"documents", "Documents"}},
        {"text", {"documents", "Documents"}},
        {"texts", {"documents", "Documents"}},
        {"paper", {"documents", "Documents"}},
        {"papers", {"documents", "Documents"}},
        {"table", {"documents", "Documents"}},
        {"tables", {"documents", "Documents"}},
        {"office file", {"documents", "Documents"}},
        {"office files", {"documents", "Documents"}},

        {"application", {"software", "Software"}},
        {"applications", {"software", "Software"}},
        {"app", {"software", "Software"}},
        {"apps", {"software", "Software"}},
        {"program", {"software", "Software"}},
        {"programs", {"software", "Software"}},
        {"update", {"software", "Software"}},
        {"updates", {"software", "Software"}},
        {"software update", {"software", "Software"}},
        {"software updates", {"software", "Software"}},
        {"patch", {"software", "Software"}},
        {"patches", {"software", "Software"}},
        {"upgrade", {"software", "Software"}},
        {"upgrades", {"software", "Software"}},
        {"updater", {"software", "Software"}},
        {"updaters", {"software", "Software"}},

        {"image file", {"images", "Images"}},
        {"image files", {"images", "Images"}},
        {"photo", {"images", "Images"}},
        {"photos", {"images", "Images"}},
        {"graphic", {"images", "Images"}},
        {"graphics", {"images", "Images"}},
        {"picture", {"images", "Images"}},
        {"pictures", {"images", "Images"}},
        {"pic", {"images", "Images"}},
        {"pics", {"images", "Images"}},
        {"screenshot", {"images", "Images"}},
        {"screenshots", {"images", "Images"}},
        {"wallpaper", {"images", "Images"}},
        {"wallpapers", {"images", "Images"}}
    };

    if (auto it = kCategorySynonyms.find(normalized_category); it != kCategorySynonyms.end()) {
        return it->second;
    }

    const std::string stripped_category = strip_trailing_stopwords(normalized_category);
    if (auto it = kCategorySynonyms.find(stripped_category); it != kCategorySynonyms.end()) {
        return it->second;
    }

    // "Media" can be broader than images, so only collapse when the paired subcategory is image-like.
    if ((normalized_category == "media" || stripped_category == "media") &&
        is_image_like_label(normalized_subcategory)) {
        return {"images", "Images"};
    }

    return {normalized_category, ""};
}

} // namespace

std::string normalize_label(const std::string& input)
{
    std::string result;
    result.reserve(input.size());

    bool last_was_space = true;
    for (unsigned char ch : input) {
        if (std::isalnum(ch)) {
            result.push_back(static_cast<char>(std::tolower(ch)));
            last_was_space = false;
        } else if (std::isspace(ch)) {
            if (!last_was_space) {
                result.push_back(' ');
                last_was_space = true;
            }
        }
    }

    while (!result.empty() && result.front() == ' ') {
        result.erase(result.begin());
    }
    while (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }
    return result;
}

std::string strip_trailing_stopwords(const std::string& normalized)
{
    if (normalized.empty()) {
        return normalized;
    }
    static const std::unordered_set<std::string> kStopwords = {
        "file", "files",
        "doc", "docs", "document", "documents",
        "image", "images",
        "photo", "photos",
        "pic", "pics"
    };

    std::istringstream iss(normalized);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    if (tokens.size() <= 1) {
        return normalized;
    }
    while (tokens.size() > 1 && kStopwords.contains(tokens.back())) {
        tokens.pop_back();
    }
    if (tokens.empty()) {
        return normalized;
    }

    std::string joined;
    for (size_t index = 0; index < tokens.size(); ++index) {
        if (index > 0) {
            joined.push_back(' ');
        }
        joined += tokens[index];
    }
    return joined;
}

NormalizedCategory normalize_category(std::string category, std::string subcategory)
{
    std::string trimmed_category = trim_copy(std::move(category));
    std::string trimmed_subcategory = trim_copy(std::move(subcategory));

    if (trimmed_category.empty()) {
        trimmed_category = "Uncategorized";
    }
    if (trimmed_subcategory.empty()) {
        trimmed_subcategory = "General";
    }

    std::string normalized_category = normalize_label(trimmed_category);
    std::string normalized_subcategory = normalize_label(trimmed_subcategory);
    const CanonicalCategoryLabel canonical_category =
        canonicalize_category_label(normalized_category, normalized_subcategory);
    normalized_category = canonical_category.normalized;
    if (!canonical_category.display.empty()) {
        trimmed_category = canonical_category.display;
    }

    if (normalized_subcategory == kLegacyInstallerBuildersNormalized) {
        trimmed_subcategory = kCanonicalInstallerToolsDisplay;
        normalized_subcategory = kCanonicalInstallerToolsNormalized;
    }

    if (const auto canonical_subcategory = canonicalize_broad_main_label(normalized_subcategory);
        canonical_subcategory.has_value() &&
        canonical_subcategory->normalized == normalized_category) {
        trimmed_subcategory = "General";
        normalized_subcategory = normalize_label(trimmed_subcategory);
    }

    if (const SemanticFamily* family_from_category = find_semantic_family(normalized_category)) {
        trimmed_category = family_from_category->canonical_category_display;
        normalized_category = family_from_category->canonical_category_normalized;
        if (is_generic_family_subcategory(normalized_subcategory,
                                          normalized_category,
                                          *family_from_category)) {
            trimmed_subcategory = "General";
            normalized_subcategory = normalize_label(trimmed_subcategory);
        }
    } else if (const SemanticFamily* family_from_subcategory =
                   find_semantic_family(normalized_subcategory);
               family_from_subcategory &&
               family_from_subcategory->parent_category_normalized == normalized_category) {
        trimmed_category = family_from_subcategory->canonical_category_display;
        normalized_category = family_from_subcategory->canonical_category_normalized;
        trimmed_subcategory = "General";
        normalized_subcategory = normalize_label(trimmed_subcategory);
    }

    return NormalizedCategory{
        trimmed_category,
        trimmed_subcategory,
        normalized_category,
        normalized_subcategory,
        strip_trailing_stopwords(normalized_subcategory)
    };
}

double string_similarity(const std::string& a, const std::string& b)
{
    if (a == b) {
        return 1.0;
    }
    if (a.empty() || b.empty()) {
        return 0.0;
    }

    const size_t m = a.size();
    const size_t n = b.size();
    std::vector<size_t> prev(n + 1), curr(n + 1);

    for (size_t j = 0; j <= n; ++j) {
        prev[j] = j;
    }

    for (size_t i = 1; i <= m; ++i) {
        curr[0] = i;
        for (size_t j = 1; j <= n; ++j) {
            size_t cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            curr[j] = std::min({prev[j] + 1, curr[j - 1] + 1, prev[j - 1] + cost});
        }
        std::swap(prev, curr);
    }

    const double dist = static_cast<double>(prev[n]);
    const double max_len = static_cast<double>(std::max(m, n));
    return 1.0 - (dist / max_len);
}

} // namespace DatabaseTaxonomyNormalizer
