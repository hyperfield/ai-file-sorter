#pragma once

#include "Types.hpp"

#include <string>
#include <unordered_set>
#include <vector>

/**
 * @brief Routes pending entries into image, document, and other analysis buckets.
 */
class AnalysisEntryRouter {
public:
    static void split_entries_for_analysis(const std::vector<FileEntry>& files,
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
                                           std::vector<FileEntry>& other_entries);
};
