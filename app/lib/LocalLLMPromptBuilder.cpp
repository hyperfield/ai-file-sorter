#include "LocalLLMPromptBuilder.hpp"

#include "FileCategoryPolicy.hpp"
#include "FolderTreeCatalog.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>
#include <utility>

namespace {

constexpr std::string_view kImageDescriptionMarker = "\nImage description: ";
constexpr std::string_view kDocumentSummaryMarker = "\nDocument summary: ";
constexpr std::string_view kRefinedSortingStyleMarker = "Sorting style: More refined";

std::string trim_copy(std::string value)
{
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

bool has_marker(std::string_view text, std::string_view marker)
{
    return text.find(marker) != std::string_view::npos;
}

std::pair<std::string, std::string> split_prompt_payload(std::string_view payload,
                                                         std::string_view marker)
{
    const auto marker_pos = payload.find(marker);
    if (marker_pos == std::string_view::npos) {
        return {std::string(payload), std::string()};
    }

    const std::size_t content_pos = marker_pos + marker.size();
    std::string base = trim_copy(std::string(payload.substr(0, marker_pos)));
    std::string content = trim_copy(std::string(payload.substr(content_pos)));
    return {std::move(base), std::move(content)};
}

std::string strip_guidance_block(std::string context,
                                 std::string_view header)
{
    const auto header_pos = context.find(header);
    if (header_pos == std::string::npos) {
        return trim_copy(std::move(context));
    }

    std::size_t erase_begin = header_pos;
    if (erase_begin >= 2 && context.substr(erase_begin - 2, 2) == "\n\n") {
        erase_begin -= 2;
    }

    std::size_t erase_end = context.find("\n\n", header_pos + header.size());
    if (erase_end == std::string::npos) {
        erase_end = context.size();
    }

    context.erase(erase_begin, erase_end - erase_begin);
    return trim_copy(std::move(context));
}

std::string strip_image_guidance_block(std::string context)
{
    constexpr std::string_view kImageGuidanceHeader = "Image categorization guidance:\n";
    return strip_guidance_block(std::move(context), kImageGuidanceHeader);
}

std::string extract_prompt_file_name(std::string_view payload)
{
    const auto newline = payload.find('\n');
    const std::string first_line = trim_copy(std::string(payload.substr(0, newline)));
    const auto slash = first_line.find_last_of("/\\");
    if (slash == std::string::npos || slash + 1 >= first_line.size()) {
        return first_line;
    }
    return first_line.substr(slash + 1);
}

bool is_refined_sorting_mode(std::string_view consistency_context)
{
    return consistency_context.find(kRefinedSortingStyleMarker) != std::string_view::npos;
}

bool is_folder_tree_mode(std::string_view consistency_context)
{
    return consistency_context.find(FolderTreeCatalog::kPromptMarker) != std::string_view::npos;
}

std::string folder_tree_system_prompt()
{
    return "You are a file organization assistant. The prompt contains an "
           "existing destination folder tree. Choose the best target folder for "
           "the item. Reply with exactly one JSON object in the format "
           "{\"targetFolder\":\"relative/folder/path\",\"createFolder\":false}. "
           "Do not return category/subcategory text, explanations, or extra lines.";
}

std::string generic_file_categorization_system_prompt()
{
    return "You are a file categorization assistant. If the file is an installer, "
           "determine the type of software it installs. Base your answer on the "
           "filename, extension, and any directory context provided. If the prompt "
           "includes an 'Allowed main categories' list, choose the main category "
           "from that list only. Use Other only when it is listed and none of the "
           "other listed main categories clearly fits. Reply with exactly one line "
           "in the format <Main category> : <Subcategory>. Main category must be "
           "broad (one or two words, plural). Subcategory must be specific, "
           "relevant, and must not repeat the main category. Do not explain your "
           "answer, add extra lines, or use label words like 'Category' or "
           "'Subcategory'. If uncertain, make your best guess from the name only.";
}

std::string document_categorization_system_prompt(bool prefer_stable_taxonomy)
{
    if (!prefer_stable_taxonomy) {
        return "You are a document categorization assistant. Categorize the "
               "document by its subject matter and content for filesystem "
               "organization, not merely by its file extension or the "
               "application that may have created it. Use any provided document "
               "summary as the primary evidence when available, and use the "
               "filename, extension, and directory context only as supporting "
               "clues. If the prompt includes an 'Allowed main categories' list, "
               "choose the main category from that list only. Otherwise choose "
               "the most semantically accurate main category and subcategory pair. "
               "Broad filesystem buckets such as Documents, Presentations, "
               "Spreadsheets, Data Exports, and Configs remain valid when they "
               "fit best, but you do not need to force every file into them. "
               "Reply with exactly one line in the format <Main category> : "
               "<Subcategory>. Main category must be broad enough to work as a "
               "folder label, and subcategory must be specific, relevant, and "
               "must not repeat the main category. Do not explain your answer, "
               "add extra lines, or use label words like 'Category' or "
               "'Subcategory'.";
    }

    return "You are a document categorization assistant. Categorize the document by "
           "its subject matter and content for filesystem organization, not merely "
           "by its file extension or the application that may have created it. Use "
           "any provided document summary as the primary evidence when available, "
           "and use the filename, extension, and directory context only as "
           "supporting clues. If the prompt includes an 'Allowed main categories' "
           "list, choose the main category from that list only. Otherwise keep the "
           "main category broad and filesystem-friendly, and use the subcategory "
           "for the specific topic. Reply with exactly one line in the format "
           "<Main category> : <Subcategory>. Main category must be broad (one or "
           "two words, plural). Subcategory must be specific, relevant, and must "
           "not repeat the main category. Do not explain your answer, add extra "
           "lines, or use label words like 'Category' or 'Subcategory'.";
}

std::string image_categorization_system_prompt(bool prefer_stable_taxonomy)
{
    if (!prefer_stable_taxonomy) {
        return "You categorize image files for filesystem organization. If the "
               "prompt includes an 'Allowed main categories' list, choose the "
               "main category from that list only. Otherwise choose the most "
               "semantically accurate main category and subcategory pair for what "
               "the image shows. You may use Images as the main category when "
               "media-type grouping is best, but you may also choose a more "
               "content-specific main category when that clearly improves "
               "organization. For screenshots and UI captures, describe the "
               "on-screen content rather than treating the image as the installer, "
               "operating system, database, or other artifact itself. Do not use "
               "Software, Operating Systems, Databases, Installers, Technology, "
               "or similar artifact categories as the main category for ordinary "
               "image files. Use any provided image description as the primary "
               "evidence, and use the filename, extension, and directory context "
               "only as supporting clues. Reply with exactly one line in the "
               "format <Main category> : <Subcategory>. The subcategory must be "
               "specific, relevant, concise, and must not repeat the main "
               "category. Do not explain your answer, add extra lines, or use "
               "label words like 'Category' or 'Subcategory'.";
    }

    return "You categorize image files for filesystem organization. If the prompt "
           "includes an 'Allowed main categories' list, choose the main category "
           "from that list only. Otherwise, because the input is an image file, "
           "always use Images as the main category. The subcategory should "
           "describe what the image shows in concise plural form when possible. For "
           "screenshots and UI captures, classify the on-screen content as an image "
           "subtype such as Dashboards, Forms, Product Pages, Interfaces, File "
           "Managers, Admin Panels, Charts, or similar. Do not use Software, "
           "Operating Systems, Databases, Installers, Technology, or similar domain "
           "categories as the main category for ordinary image files. Use any "
           "provided image description as the primary evidence, and use the "
           "filename, extension, and directory context only as supporting clues. "
           "Reply with exactly one line in the format Images : <Subcategory>. The "
           "subcategory must be specific, relevant, concise, and must not repeat "
           "Images. Do not explain your answer, add extra lines, or use label words "
           "like 'Category' or 'Subcategory'.";
}

std::string directory_categorization_system_prompt()
{
    return "You are a directory categorization assistant. Categorize the directory "
           "by the overall theme suggested by its name and path context. If the "
           "prompt includes an 'Allowed main categories' list, choose the main "
           "category from that list only. Reply with exactly one line in the format "
           "<Main category> : <Subcategory>. Main category must be broad (one or "
           "two words, plural). Subcategory must be specific, relevant, and must "
           "not repeat the main category. Do not explain your answer, add extra "
           "lines, or use label words like 'Category' or 'Subcategory'.";
}

std::string build_generic_categorization_user_prompt(const std::string& file_name,
                                                     const std::string& file_path,
                                                     FileType file_type,
                                                     const std::string& consistency_context)
{
    std::ostringstream prompt;
    prompt << (file_type == FileType::File ? "Categorize this file.\n" : "Categorize this directory.\n");
    if (!file_path.empty()) {
        prompt << "Full path: " << file_path << "\n";
    }
    prompt << (file_type == FileType::File ? "File name: " : "Directory name: ")
           << file_name << "\n";

    if (!consistency_context.empty()) {
        prompt << "\n" << consistency_context << "\n";
    }

    if (is_folder_tree_mode(consistency_context)) {
        prompt << "\nAnswer with exactly one JSON object:\n"
               << "{\"targetFolder\":\"relative/folder/path\",\"createFolder\":false}";
    } else {
        prompt << "\nAnswer with exactly one line:\n<Main category> : <Subcategory>";
    }
    return prompt.str();
}

std::string build_image_categorization_user_prompt(const std::string& file_name,
                                                   const std::string& file_path,
                                                   const std::string& consistency_context)
{
    const auto [base_path, image_description] = split_prompt_payload(file_path, kImageDescriptionMarker);
    const std::string extra_context = strip_image_guidance_block(consistency_context);
    const bool prefer_stable_taxonomy = !is_refined_sorting_mode(consistency_context);

    std::ostringstream prompt;
    prompt << "Categorize this image file for file organization.\n\n";
    prompt << "File name: " << file_name << "\n";
    if (!base_path.empty()) {
        prompt << "Path: " << base_path << "\n";
    }
    if (!image_description.empty()) {
        prompt << "Image description: " << image_description << "\n";
    }
    if (!extra_context.empty()) {
        prompt << "\n" << extra_context << "\n";
    }
    if (is_folder_tree_mode(consistency_context)) {
        prompt << "\nAnswer with exactly one JSON object:\n"
               << "{\"targetFolder\":\"relative/folder/path\",\"createFolder\":false}";
    } else if (prefer_stable_taxonomy) {
        prompt << "\nAnswer with exactly one line:\nImages : <Subcategory>";
    } else {
        prompt << "\nAnswer with exactly one line:\n<Main category> : <Subcategory>";
    }
    return prompt.str();
}

std::string build_document_categorization_user_prompt(const std::string& file_name,
                                                      const std::string& file_path,
                                                      const std::string& consistency_context)
{
    const auto [base_path, document_summary] = split_prompt_payload(file_path, kDocumentSummaryMarker);

    std::ostringstream prompt;
    prompt << "Categorize this document file for file organization.\n\n";
    prompt << "File name: " << file_name << "\n";
    if (!base_path.empty()) {
        prompt << "Path: " << base_path << "\n";
    }
    if (!document_summary.empty()) {
        prompt << "Document summary: " << document_summary << "\n";
    }
    if (!consistency_context.empty()) {
        prompt << "\n" << consistency_context << "\n";
    }
    if (is_folder_tree_mode(consistency_context)) {
        prompt << "\nAnswer with exactly one JSON object:\n"
               << "{\"targetFolder\":\"relative/folder/path\",\"createFolder\":false}";
    } else {
        prompt << "\nAnswer with exactly one line:\n<Main category> : <Subcategory>";
    }
    return prompt.str();
}

std::string truncate_with_ellipsis(std::string value, std::size_t limit)
{
    if (value.size() <= limit) {
        return value;
    }
    if (limit <= 3) {
        value.resize(limit);
        return value;
    }
    value.resize(limit - 3);
    value += "...";
    return value;
}

} // namespace

namespace LocalLLMPromptBuilder {

std::string build_system_prompt(std::string_view file_path,
                                FileType file_type,
                                std::string_view consistency_context)
{
    const bool prefer_stable_taxonomy = !is_refined_sorting_mode(consistency_context);
    if (is_folder_tree_mode(consistency_context)) {
        return folder_tree_system_prompt();
    }
    if (file_type == FileType::Directory) {
        return directory_categorization_system_prompt();
    }
    if (has_marker(file_path, kImageDescriptionMarker)) {
        return image_categorization_system_prompt(prefer_stable_taxonomy);
    }
    if (FileCategoryPolicy::is_supported_document_file_name(extract_prompt_file_name(file_path))) {
        return document_categorization_system_prompt(prefer_stable_taxonomy);
    }
    return generic_file_categorization_system_prompt();
}

std::string build_user_prompt(const std::string& file_name,
                              const std::string& file_path,
                              FileType file_type,
                              const std::string& consistency_context)
{
    if (file_type == FileType::File && has_marker(file_path, kImageDescriptionMarker)) {
        return build_image_categorization_user_prompt(file_name, file_path, consistency_context);
    }
    if (file_type == FileType::File &&
        FileCategoryPolicy::is_supported_document_file_name(file_name)) {
        return build_document_categorization_user_prompt(file_name, file_path, consistency_context);
    }
    return build_generic_categorization_user_prompt(file_name,
                                                    file_path,
                                                    file_type,
                                                    consistency_context);
}

int prompt_token_budget(int context_tokens, int max_tokens)
{
    if (context_tokens <= 0) {
        return 0;
    }
    const int generation_reserve = std::max(32, max_tokens);
    return std::max(1, context_tokens - generation_reserve);
}

std::string shrink_user_prompt_for_context(const std::string& prompt, int attempt)
{
    static const std::array<std::size_t, 4> kSummaryBudgets = {1200, 800, 500, 300};
    static const std::array<std::size_t, 4> kPromptBudgets = {1800, 1400, 1000, 700};

    auto trim_labeled_section = [&](std::string text, std::string_view marker) {
        const auto marker_pos = text.find(marker);
        if (marker_pos == std::string::npos) {
            return text;
        }

        std::size_t suffix_start = std::string::npos;
        constexpr std::array<std::string_view, 8> kSectionMarkers = {
            "\nFile name:",
            "\nDirectory name:",
            "\nPath:",
            "\nAllowed main categories",
            "\nAllowed subcategories",
            "\nDetermine the canonical main category and subcategory",
            "\nRecent assignments for similar items:",
            "\nAnswer with exactly one line:"
        };
        for (const std::string_view section_marker : kSectionMarkers) {
            const auto section_pos = text.find(section_marker, marker_pos + 1);
            if (section_pos != std::string::npos) {
                suffix_start = suffix_start == std::string::npos
                    ? section_pos
                    : std::min(suffix_start, section_pos);
            }
        }
        if (suffix_start == std::string::npos) {
            suffix_start = text.size();
        }

        const std::size_t section_start = marker_pos + marker.size();
        const std::size_t section_length = suffix_start > section_start ? suffix_start - section_start : 0;
        const std::size_t budget = kSummaryBudgets[std::min<std::size_t>(
            static_cast<std::size_t>(attempt),
            kSummaryBudgets.size() - 1)];
        if (section_length <= budget) {
            return text;
        }

        const std::string prefix = text.substr(0, section_start);
        const std::string section = truncate_with_ellipsis(text.substr(section_start, section_length), budget);
        const std::string suffix = text.substr(suffix_start);
        return prefix + section + suffix;
    };

    std::string trimmed = prompt;
    trimmed = trim_labeled_section(std::move(trimmed), "\nDocument summary: ");
    trimmed = trim_labeled_section(std::move(trimmed), "\nImage description: ");

    const std::size_t budget = kPromptBudgets[std::min<std::size_t>(
        static_cast<std::size_t>(attempt),
        kPromptBudgets.size() - 1)];
    if (trimmed.size() > budget) {
        return truncate_with_ellipsis(trimmed, budget);
    }

    return trimmed;
}

} // namespace LocalLLMPromptBuilder
