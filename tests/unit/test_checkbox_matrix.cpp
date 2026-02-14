#include <catch2/catch_test_macros.hpp>

#include "DocumentTextAnalyzer.hpp"
#include "LlavaImageAnalyzer.hpp"
#include "MainAppTestAccess.hpp"
#include "Types.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

struct Combo {
    bool analyze_images;
    bool analyze_documents;
    bool process_images_only;
    bool process_documents_only;
    bool rename_images_only;
    bool rename_documents_only;
    bool categorize_files;
};

enum class Bucket {
    None,
    Image,
    Document,
    Other
};

std::string combo_label(const Combo& combo) {
    std::ostringstream out;
    out << "AI=" << combo.analyze_images
        << " AD=" << combo.analyze_documents
        << " PI=" << combo.process_images_only
        << " PD=" << combo.process_documents_only
        << " RI=" << combo.rename_images_only
        << " RD=" << combo.rename_documents_only
        << " CF=" << combo.categorize_files;
    return out.str();
}

bool contains_name(const std::vector<FileEntry>& entries, const std::string& name) {
    return std::any_of(entries.begin(), entries.end(), [&name](const FileEntry& entry) {
        return entry.file_name == name;
    });
}

Bucket expected_bucket(const FileEntry& entry,
                       const Combo& combo,
                       const std::unordered_set<std::string>& renamed_files) {
    const bool restrict_types = combo.process_images_only || combo.process_documents_only;
    const bool allow_images = !restrict_types || combo.process_images_only;
    const bool allow_documents = !restrict_types || combo.process_documents_only;
    const bool allow_other_files = combo.categorize_files && !restrict_types;

    if (entry.type == FileType::Directory) {
        return restrict_types ? Bucket::None : Bucket::Other;
    }

    const bool is_image = LlavaImageAnalyzer::is_supported_image(entry.full_path);
    const bool is_document = DocumentTextAnalyzer::is_supported_document(entry.full_path);

    if (is_image) {
        if (!allow_images) {
            return Bucket::None;
        }
        if (combo.analyze_images) {
            const bool already_renamed = renamed_files.contains(entry.file_name);
            if (already_renamed) {
                return combo.rename_images_only ? Bucket::None : Bucket::Other;
            }
            return Bucket::Image;
        }
        return allow_other_files ? Bucket::Other : Bucket::None;
    }

    if (is_document) {
        if (!allow_documents) {
            return Bucket::None;
        }
        if (combo.analyze_documents) {
            const bool already_renamed = renamed_files.contains(entry.file_name);
            if (already_renamed) {
                return combo.rename_documents_only ? Bucket::None : Bucket::Other;
            }
            return Bucket::Document;
        }
        return allow_other_files ? Bucket::Other : Bucket::None;
    }

    return allow_other_files ? Bucket::Other : Bucket::None;
}

Bucket actual_bucket(const FileEntry& entry,
                     const std::vector<FileEntry>& image_entries,
                     const std::vector<FileEntry>& document_entries,
                     const std::vector<FileEntry>& other_entries) {
    const bool in_images = contains_name(image_entries, entry.file_name);
    const bool in_docs = contains_name(document_entries, entry.file_name);
    const bool in_other = contains_name(other_entries, entry.file_name);

    const int count = static_cast<int>(in_images) + static_cast<int>(in_docs) + static_cast<int>(in_other);
    CHECK(count <= 1);

    if (in_images) {
        return Bucket::Image;
    }
    if (in_docs) {
        return Bucket::Document;
    }
    if (in_other) {
        return Bucket::Other;
    }
    return Bucket::None;
}

void run_combo_matrix(const std::vector<FileEntry>& files,
                      const std::unordered_set<std::string>& renamed_files,
                      bool renamed_label) {
    for (int mask = 0; mask < 128; ++mask) {
        Combo combo{
            static_cast<bool>(mask & (1 << 0)),
            static_cast<bool>(mask & (1 << 1)),
            static_cast<bool>(mask & (1 << 2)),
            static_cast<bool>(mask & (1 << 3)),
            static_cast<bool>(mask & (1 << 4)),
            static_cast<bool>(mask & (1 << 5)),
            static_cast<bool>(mask & (1 << 6))
        };

        std::vector<FileEntry> image_entries;
        std::vector<FileEntry> document_entries;
        std::vector<FileEntry> other_entries;

        MainAppTestAccess::split_entries_for_analysis(files,
                                                      combo.analyze_images,
                                                      combo.analyze_documents,
                                                      combo.process_images_only,
                                                      combo.process_documents_only,
                                                      combo.rename_images_only,
                                                      combo.rename_documents_only,
                                                      combo.categorize_files,
                                                      false,
                                                      renamed_files,
                                                      image_entries,
                                                      document_entries,
                                                      other_entries);

        std::cout << "[combo] " << combo_label(combo)
                  << " renamed=" << renamed_label
                  << " -> img=" << image_entries.size()
                  << " doc=" << document_entries.size()
                  << " other=" << other_entries.size()
                  << std::endl;

        INFO(combo_label(combo));

        for (const auto& entry : files) {
            const Bucket expected = expected_bucket(entry, combo, renamed_files);
            const Bucket actual = actual_bucket(entry, image_entries, document_entries, other_entries);
            CHECK(expected == actual);
        }

        for (const auto& entry : image_entries) {
            CHECK(LlavaImageAnalyzer::is_supported_image(entry.full_path));
        }
        for (const auto& entry : document_entries) {
            CHECK(DocumentTextAnalyzer::is_supported_document(entry.full_path));
        }
    }
}

} // namespace

TEST_CASE("Checkbox combinations route entries without renamed files") {
    const std::vector<FileEntry> files = {
        {"/tmp/photo.png", "photo.png", FileType::File},
        {"/tmp/report.pdf", "report.pdf", FileType::File},
        {"/tmp/archive.bin", "archive.bin", FileType::File},
        {"/tmp/folder", "folder", FileType::Directory}
    };

    std::unordered_set<std::string> renamed_files;
    run_combo_matrix(files, renamed_files, false);
}

TEST_CASE("Checkbox combinations route entries with renamed files") {
    const std::vector<FileEntry> files = {
        {"/tmp/photo.png", "photo.png", FileType::File},
        {"/tmp/report.pdf", "report.pdf", FileType::File},
        {"/tmp/archive.bin", "archive.bin", FileType::File},
        {"/tmp/folder", "folder", FileType::Directory}
    };

    std::unordered_set<std::string> renamed_files = {"photo.png", "report.pdf"};
    run_combo_matrix(files, renamed_files, true);
}
