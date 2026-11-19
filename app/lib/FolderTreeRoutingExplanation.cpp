#include "FolderTreeRoutingExplanation.hpp"

#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

void add_line(std::vector<std::string>& lines, std::string line)
{
    if (!line.empty()) {
        lines.push_back(std::move(line));
    }
}

std::string join_lines(const std::vector<std::string>& lines)
{
    std::ostringstream out;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (index > 0) {
            out << '\n';
        }
        out << lines[index];
    }
    return out.str();
}

std::string semantic_category_line(const FolderTreeRoutingExplanation::Context& context)
{
    if (context.semantic_category.empty() && context.semantic_subcategory.empty()) {
        return {};
    }
    if (context.semantic_subcategory.empty()) {
        return "AI category: " + context.semantic_category + ".";
    }
    return "AI category: " + context.semantic_category + " / " +
           context.semantic_subcategory + ".";
}

std::string convention_line(const FolderStructurePattern::Profile& profile)
{
    if (profile.has_johnny_decimal_like_ranges) {
        return "Archive pattern: Johnny.Decimal-like area/category ranges detected.";
    }
    if (profile.has_numbered_prefixes) {
        return "Archive pattern: numbered folder prefixes detected.";
    }
    if (profile.has_alphabetic_prefixes) {
        return "Archive pattern: alphabetic folder-code prefixes detected.";
    }
    if (profile.has_recognized_conventions) {
        return "Archive pattern: existing folder naming conventions detected.";
    }
    return {};
}

std::string readable_target_line(const std::string& relative_path)
{
    const std::string readable = FolderStructurePattern::human_label_path(relative_path);
    if (readable.empty() || readable == relative_path) {
        return {};
    }
    return "Readable target: " + readable + ".";
}

std::string closest_existing_line(
    const FolderTreeCatalog::Selection& selection,
    const std::optional<FolderTreeCatalog::SemanticMatch>& best_existing)
{
    if (!best_existing) {
        return {};
    }

    std::ostringstream out;
    if (best_existing->entry.relative_path == selection.relative_path) {
        out << "Matched existing folder: ";
    } else {
        out << "Closest existing folder: ";
    }
    out << best_existing->entry.relative_path << " (score " << best_existing->score << ").";
    return out.str();
}

std::string decision_line(const FolderTreeCatalog::Selection& selection)
{
    if (selection.relative_path.empty()) {
        return {};
    }
    if (selection.exists) {
        return "Decision: use existing target " + selection.relative_path + ".";
    }
    if (selection.suggested_new) {
        return "Decision: suggest new target " + selection.relative_path +
               "; it will be created if approved.";
    }
    return "Decision: use target " + selection.relative_path + ".";
}

} // namespace

namespace FolderTreeRoutingExplanation {

std::string build(const Context& context)
{
    std::vector<std::string> lines;
    lines.reserve(6);

    add_line(lines, semantic_category_line(context));
    if (!context.semantic_target_folder.empty() &&
        context.semantic_target_folder != context.selection.relative_path) {
        add_line(lines, "Semantic target: " + context.semantic_target_folder + ".");
    }
    add_line(lines, convention_line(context.structure_profile));
    add_line(lines, readable_target_line(context.selection.relative_path));
    add_line(lines, closest_existing_line(context.selection, context.best_existing));
    add_line(lines, decision_line(context.selection));

    return join_lines(lines);
}

} // namespace FolderTreeRoutingExplanation
