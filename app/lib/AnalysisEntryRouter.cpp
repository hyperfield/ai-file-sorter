#include "AnalysisEntryRouter.hpp"

#include "DocumentTextAnalyzer.hpp"
#include "LlavaImageAnalyzer.hpp"

void AnalysisEntryRouter::split_entries_for_analysis(
    const std::vector<FileEntry>& files,
    bool analyze_images,
    bool analyze_documents,
    bool process_images_only,
    bool process_documents_only,
    bool rename_images_only,
    bool rename_documents_only,
    bool categorize_files,
    bool use_full_path_keys,
    const std::unordered_set<std::string>& renamed_files,
    std::vector<FileEntry>& image_entries,
    std::vector<FileEntry>& document_entries,
    std::vector<FileEntry>& other_entries)
{
    image_entries.clear();
    document_entries.clear();
    other_entries.clear();
    image_entries.reserve(files.size());
    document_entries.reserve(files.size());
    other_entries.reserve(files.size());

    const bool restrict_types = process_images_only || process_documents_only;
    const bool allow_images = !restrict_types || process_images_only;
    const bool allow_documents = !restrict_types || process_documents_only;
    const bool allow_other_files = categorize_files && !restrict_types;

    for (const auto& entry : files) {
        const std::string entry_key = use_full_path_keys ? entry.full_path : entry.file_name;
        if (entry.type == FileType::Directory) {
            if (!restrict_types) {
                other_entries.push_back(entry);
            }
            continue;
        }
        const bool is_image_entry = entry.type == FileType::File &&
                                    LlavaImageAnalyzer::is_supported_image(entry.full_path);
        const bool is_document_entry = entry.type == FileType::File &&
                                       DocumentTextAnalyzer::is_supported_document(entry.full_path);

        if (is_image_entry) {
            if (!allow_images) {
                continue;
            }
            if (analyze_images) {
                const bool already_renamed = renamed_files.contains(entry_key);
                if (already_renamed) {
                    if (rename_images_only) {
                        continue;
                    }
                    other_entries.push_back(entry);
                } else {
                    image_entries.push_back(entry);
                }
            } else if (allow_other_files) {
                other_entries.push_back(entry);
            }
            continue;
        }

        if (is_document_entry) {
            if (!allow_documents) {
                continue;
            }
            if (analyze_documents) {
                const bool already_renamed = renamed_files.contains(entry_key);
                if (already_renamed) {
                    if (rename_documents_only) {
                        continue;
                    }
                    other_entries.push_back(entry);
                } else {
                    document_entries.push_back(entry);
                }
            } else if (allow_other_files) {
                other_entries.push_back(entry);
            }
            continue;
        }

        if (allow_other_files) {
            other_entries.push_back(entry);
        }
    }
}
