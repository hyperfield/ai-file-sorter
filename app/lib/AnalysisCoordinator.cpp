#include "AnalysisCoordinator.hpp"

#include "AnalysisEntryRouter.hpp"
#include "CategorizationProgressDialog.hpp"
#include "DocumentTextAnalyzer.hpp"
#include "ImageRenameMetadataService.hpp"
#include "LlavaImageAnalyzer.hpp"
#include "MainApp.hpp"
#include "MediaRenameMetadataService.hpp"
#include "Utils.hpp"
#include "VisualLlmRuntime.hpp"

#include <QMessageBox>
#include <QMetaObject>
#include <QThread>
#include <QByteArray>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

std::string to_utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

std::optional<bool> read_env_bool(const char* key)
{
    const char* value = std::getenv(key);
    if (!value || value[0] == '\0') {
        return std::nullopt;
    }
    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on") {
        return true;
    }
    if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off") {
        return false;
    }
    return std::nullopt;
}

std::optional<int> read_env_int(const char* key)
{
    const char* value = std::getenv(key);
    if (!value || value[0] == '\0') {
        return std::nullopt;
    }
    char* end_ptr = nullptr;
    long parsed = std::strtol(value, &end_ptr, 10);
    if (end_ptr == value || (end_ptr && *end_ptr != '\0')) {
        return std::nullopt;
    }
    if (parsed <= 0 || parsed > 100000) {
        return std::nullopt;
    }
    return static_cast<int>(parsed);
}

int resolve_local_context_tokens()
{
    if (auto parsed = read_env_int("AI_FILE_SORTER_CTX_TOKENS")) {
        return *parsed;
    }
    if (auto parsed = read_env_int("LLAMA_CPP_MAX_CONTEXT")) {
        return *parsed;
    }
    return 2048;
}

size_t resolve_document_char_budget(bool using_local_llm, int max_output_tokens)
{
    int context_tokens = using_local_llm ? resolve_local_context_tokens() : 4096;
    context_tokens = std::clamp(context_tokens, 512, 8192);
    const int reserve_tokens = std::max(192, context_tokens / 6);
    const int output_tokens = std::clamp(max_output_tokens, 0, context_tokens / 2);
    const int prompt_tokens = std::max(256, context_tokens - reserve_tokens - output_tokens);
    const size_t chars_per_token = using_local_llm ? 2 : 4;
    return static_cast<size_t>(prompt_tokens) * chars_per_token;
}

} // namespace

AnalysisCoordinator::AnalysisCoordinator(MainApp& app)
    : app_(app)
{
}

std::string AnalysisCoordinator::resolve_document_prompt_name(const std::string& original_name,
                                                              const std::string& suggested_name)
{
    return suggested_name.empty() ? original_name : suggested_name;
}

std::string AnalysisCoordinator::build_document_prompt_path(const std::string& full_path,
                                                            const std::string& prompt_name,
                                                            const std::string& summary)
{
    const auto entry_path = Utils::utf8_to_path(full_path);
    const std::string effective_name =
        prompt_name.empty() ? Utils::path_to_utf8(entry_path.filename()) : prompt_name;
    std::string prompt_path = Utils::path_to_utf8(
        entry_path.parent_path() / Utils::utf8_to_path(effective_name));
    if (!summary.empty()) {
        prompt_path += "\nDocument summary: " + summary;
    }
    return prompt_path;
}

void AnalysisCoordinator::execute()
{
    const std::string directory_path = app_.get_folder_path();
    app_.core_logger->info("Starting analysis for directory '{}'", directory_path);

    bool stop_requested = false;
    auto update_stop = [this, &stop_requested]() {
        if (!stop_requested && app_.should_abort_analysis()) {
            stop_requested = true;
        }
        return stop_requested;
    };

    app_.append_progress(to_utf8(app_.tr("[SCAN] Exploring %1")
                                     .arg(QString::fromStdString(directory_path))));
    update_stop();

    try {
        app_.prune_empty_cached_entries_for(directory_path);
        const bool analyze_images = app_.settings.get_analyze_images_by_content();
        const bool analyze_documents = app_.settings.get_analyze_documents_by_content();
        const bool process_images_only = analyze_images && app_.settings.get_process_images_only();
        const bool process_documents_only = analyze_documents && app_.settings.get_process_documents_only();
        const bool rename_images_only = analyze_images && app_.settings.get_rename_images_only();
        const bool rename_documents_only = analyze_documents && app_.settings.get_rename_documents_only();
        const bool allow_image_renames = app_.settings.get_offer_rename_images();
        const bool allow_document_renames = app_.settings.get_offer_rename_documents();
        const bool offer_image_renames = analyze_images && allow_image_renames;
        const bool offer_document_renames = analyze_documents && allow_document_renames;
        const bool wants_visual_rename = analyze_images && allow_image_renames && !rename_images_only;
        const bool wants_document_rename =
            analyze_documents && allow_document_renames && !rename_documents_only;
        const bool add_image_date_place_prefixes =
            analyze_images &&
            allow_image_renames &&
            app_.settings.get_add_image_date_place_to_filename();
        const bool add_image_date_to_category =
            analyze_images &&
            app_.settings.get_add_image_date_to_category();
        const bool add_audio_video_metadata_to_filename =
            app_.settings.get_add_audio_video_metadata_to_filename();
        const bool add_document_date =
            analyze_documents && app_.settings.get_add_document_date_to_category();
        const bool use_full_path_keys = app_.settings.get_include_subdirectories();

        const auto cached_entries = app_.categorization_service.load_cached_entries(directory_path);
        std::vector<CategorizedFile> pending_renames;
        pending_renames.reserve(cached_entries.size());
        std::unordered_set<std::string> renamed_files;
        app_.already_categorized_files.clear();
        app_.already_categorized_files.reserve(cached_entries.size());
        std::vector<FileEntry> cached_image_entries_for_visual;
        std::unordered_map<std::string, size_t> cached_visual_indices;
        std::vector<FileEntry> cached_document_entries_for_analysis;
        std::unordered_map<std::string, size_t> cached_document_indices;
        std::unordered_map<std::string, std::string> cached_image_suggestions;
        std::unordered_map<std::string, std::string> cached_document_suggestions;
        if (wants_visual_rename) {
            cached_image_entries_for_visual.reserve(cached_entries.size());
            cached_visual_indices.reserve(cached_entries.size());
        }
        if (wants_document_rename) {
            cached_document_entries_for_analysis.reserve(cached_entries.size());
            cached_document_indices.reserve(cached_entries.size());
        }

        auto to_lower = [](std::string value) {
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            return value;
        };
        auto has_category = [](const CategorizedFile& entry) {
            return !entry.category.empty() && !entry.subcategory.empty();
        };
        auto is_supported_image_entry = [](const CategorizedFile& entry) {
            if (entry.type != FileType::File) {
                return false;
            }
            const auto full_path = Utils::utf8_to_path(entry.file_path) /
                                   Utils::utf8_to_path(entry.file_name);
            return LlavaImageAnalyzer::is_supported_image(full_path);
        };
        auto is_supported_document_entry = [](const CategorizedFile& entry) {
            if (entry.type != FileType::File) {
                return false;
            }
            const auto full_path = Utils::utf8_to_path(entry.file_path) /
                                   Utils::utf8_to_path(entry.file_name);
            return DocumentTextAnalyzer::is_supported_document(full_path);
        };
        auto is_missing_category_label = [](const std::string& value) {
            std::string trimmed = value;
            const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
            trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), not_space));
            trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), not_space).base(), trimmed.end());
            if (trimmed.empty()) {
                return true;
            }
            std::transform(trimmed.begin(), trimmed.end(), trimmed.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            return trimmed == "uncategorized";
        };
        auto file_key = [use_full_path_keys](const CategorizedFile& entry) {
            if (!use_full_path_keys) {
                return entry.file_name;
            }
            const auto full_path = Utils::utf8_to_path(entry.file_path) /
                                   Utils::utf8_to_path(entry.file_name);
            return Utils::path_to_utf8(full_path);
        };
        auto entry_key = [use_full_path_keys](const FileEntry& entry) {
            return use_full_path_keys ? entry.full_path : entry.file_name;
        };
        auto resolve_entry_for_storage = [this](const CategorizedFile& entry) {
            const std::string canonical_category =
                entry.canonical_category.empty() ? entry.category : entry.canonical_category;
            const std::string canonical_subcategory =
                entry.canonical_subcategory.empty() ? entry.subcategory : entry.canonical_subcategory;
            if (!canonical_category.empty()) {
                return app_.db_manager.resolve_category(canonical_category, canonical_subcategory);
            }
            return app_.db_manager.resolve_category_for_language(entry.category,
                                                                 entry.subcategory,
                                                                 app_.settings.get_category_language());
        };
        auto persist_rename_only_progress =
            [this, &is_missing_category_label](const FileEntry& entry, const std::string& suggested_name) {
                const auto entry_path = Utils::utf8_to_path(entry.full_path);
                const std::string dir_path = Utils::path_to_utf8(entry_path.parent_path());
                const auto cached_entry =
                    app_.db_manager.get_categorized_file(dir_path, entry.file_name, entry.type);

                std::string category;
                std::string subcategory;
                bool used_consistency = false;
                DatabaseManager::ResolvedCategory resolved{0, "", ""};

                if (cached_entry) {
                    category = cached_entry->category;
                    subcategory = cached_entry->subcategory;
                    if (is_missing_category_label(category)) {
                        category.clear();
                    }
                    if (is_missing_category_label(subcategory)) {
                        subcategory.clear();
                    }
                    if (!category.empty()) {
                        resolved.category = category;
                        resolved.subcategory = subcategory;
                        resolved.taxonomy_id = cached_entry->taxonomy_id;
                    }
                    used_consistency = cached_entry->used_consistency_hints;
                }

                const std::string file_type_label = (entry.type == FileType::Directory) ? "D" : "F";
                app_.db_manager.insert_or_update_file_with_categorization(entry.file_name,
                                                                          file_type_label,
                                                                          dir_path,
                                                                          resolved,
                                                                          used_consistency,
                                                                          suggested_name,
                                                                          true);
            };
        auto persist_llm_suggestion_progress =
            [this, &is_missing_category_label](const FileEntry& entry, const std::string& suggested_name) {
                if (suggested_name.empty()) {
                    return;
                }
                const auto entry_path = Utils::utf8_to_path(entry.full_path);
                const std::string dir_path = Utils::path_to_utf8(entry_path.parent_path());
                const auto cached_entry =
                    app_.db_manager.get_categorized_file(dir_path, entry.file_name, entry.type);

                std::string category;
                std::string subcategory;
                bool used_consistency = false;
                bool rename_applied = false;
                DatabaseManager::ResolvedCategory resolved{0, "", ""};

                if (cached_entry) {
                    category = cached_entry->category;
                    subcategory = cached_entry->subcategory;
                    if (is_missing_category_label(category)) {
                        category.clear();
                    }
                    if (is_missing_category_label(subcategory)) {
                        subcategory.clear();
                    }
                    if (!category.empty()) {
                        resolved.category = category;
                        resolved.subcategory = subcategory;
                        resolved.taxonomy_id = cached_entry->taxonomy_id;
                    }
                    used_consistency = cached_entry->used_consistency_hints;
                    rename_applied = cached_entry->rename_applied;
                }

                const std::string file_type_label = (entry.type == FileType::Directory) ? "D" : "F";
                app_.db_manager.insert_or_update_file_with_categorization(entry.file_name,
                                                                          file_type_label,
                                                                          dir_path,
                                                                          resolved,
                                                                          used_consistency,
                                                                          suggested_name,
                                                                          false,
                                                                          rename_applied);
            };
        auto persist_cached_suggestion =
            [this, &resolve_entry_for_storage](const CategorizedFile& entry, const std::string& suggested_name) {
                DatabaseManager::ResolvedCategory resolved = resolve_entry_for_storage(entry);
                const std::string file_type_label = (entry.type == FileType::Directory) ? "D" : "F";
                app_.db_manager.insert_or_update_file_with_categorization(entry.file_name,
                                                                          file_type_label,
                                                                          entry.file_path,
                                                                          resolved,
                                                                          entry.used_consistency_hints,
                                                                          suggested_name,
                                                                          entry.rename_only,
                                                                          entry.rename_applied);
            };
        auto persist_analysis_results =
            [this, &is_missing_category_label, &resolve_entry_for_storage](
                const std::vector<CategorizedFile>& entries) {
                for (const auto& entry : entries) {
                    std::string category = entry.category;
                    std::string subcategory = entry.subcategory;
                    if (is_missing_category_label(category)) {
                        category.clear();
                    }
                    if (is_missing_category_label(subcategory)) {
                        subcategory.clear();
                    }
                    if (category.empty() && subcategory.empty() && entry.suggested_name.empty()) {
                        continue;
                    }

                    DatabaseManager::ResolvedCategory resolved{0, "", ""};
                    if (!category.empty()) {
                        resolved = resolve_entry_for_storage(entry);
                    }

                    const std::string file_type_label = (entry.type == FileType::Directory) ? "D" : "F";
                    app_.db_manager.insert_or_update_file_with_categorization(entry.file_name,
                                                                              file_type_label,
                                                                              entry.file_path,
                                                                              resolved,
                                                                              entry.used_consistency_hints,
                                                                              entry.suggested_name,
                                                                              entry.rename_only,
                                                                              entry.rename_applied);
                }
            };

        for (const auto& cached_entry : cached_entries) {
            auto entry = cached_entry;
            const bool is_image_entry = is_supported_image_entry(entry);
            const bool is_document_entry = is_supported_document_entry(entry);
            const bool allow_entry_renames =
                (is_image_entry && allow_image_renames) ||
                (is_document_entry && allow_document_renames);
            const bool suggested_matches =
                !entry.suggested_name.empty() && to_lower(entry.suggested_name) == to_lower(entry.file_name);
            const bool already_renamed = entry.rename_applied || suggested_matches;
            if (already_renamed) {
                renamed_files.insert(file_key(entry));
            }
            if (entry.rename_only && !has_category(entry)) {
                if (!allow_entry_renames) {
                    continue;
                }
                if (!already_renamed) {
                    pending_renames.push_back(entry);
                    if (is_image_entry && !rename_images_only && !entry.suggested_name.empty()) {
                        cached_image_suggestions.emplace(file_key(entry), entry.suggested_name);
                    }
                    if (is_document_entry && !rename_documents_only && !entry.suggested_name.empty()) {
                        cached_document_suggestions.emplace(file_key(entry), entry.suggested_name);
                    }
                }
                continue;
            }
            if (!has_category(entry)) {
                if (!entry.suggested_name.empty()) {
                    if (is_image_entry) {
                        cached_image_suggestions.emplace(file_key(entry), entry.suggested_name);
                    }
                    if (is_document_entry) {
                        cached_document_suggestions.emplace(file_key(entry), entry.suggested_name);
                    }
                    if (!already_renamed &&
                        allow_entry_renames &&
                        ((rename_images_only && analyze_images && is_image_entry) ||
                         (rename_documents_only && analyze_documents && is_document_entry))) {
                        CategorizedFile adjusted = entry;
                        adjusted.rename_only = true;
                        pending_renames.push_back(std::move(adjusted));
                    }
                }
                continue;
            }
            if (!allow_entry_renames) {
                entry.suggested_name.clear();
                entry.rename_only = false;
            }
            if (rename_images_only && analyze_images && is_image_entry) {
                if (!already_renamed && entry.suggested_name.empty()) {
                    continue;
                }
                CategorizedFile adjusted = entry;
                adjusted.rename_only = true;
                app_.already_categorized_files.push_back(std::move(adjusted));
                continue;
            }
            if (rename_documents_only && analyze_documents && is_document_entry) {
                if (!already_renamed && entry.suggested_name.empty()) {
                    continue;
                }
                CategorizedFile adjusted = entry;
                adjusted.rename_only = true;
                app_.already_categorized_files.push_back(std::move(adjusted));
                continue;
            }
            if (wants_visual_rename && is_image_entry && entry.suggested_name.empty() && !already_renamed) {
                const auto entry_index = app_.already_categorized_files.size();
                app_.already_categorized_files.push_back(entry);
                cached_visual_indices.emplace(file_key(entry), entry_index);
                const auto full_path = Utils::utf8_to_path(entry.file_path) /
                                       Utils::utf8_to_path(entry.file_name);
                cached_image_entries_for_visual.push_back(
                    FileEntry{Utils::path_to_utf8(full_path), entry.file_name, entry.type});
                continue;
            }
            if (wants_document_rename && is_document_entry && entry.suggested_name.empty() && !already_renamed) {
                const auto entry_index = app_.already_categorized_files.size();
                app_.already_categorized_files.push_back(entry);
                cached_document_indices.emplace(file_key(entry), entry_index);
                const auto full_path = Utils::utf8_to_path(entry.file_path) /
                                       Utils::utf8_to_path(entry.file_name);
                cached_document_entries_for_analysis.push_back(
                    FileEntry{Utils::path_to_utf8(full_path), entry.file_name, entry.type});
                continue;
            }
            app_.already_categorized_files.push_back(entry);
        }

        if (process_images_only || process_documents_only) {
            const bool allow_images = process_images_only;
            const bool allow_documents = process_documents_only;
            auto filter_entries = [&](std::vector<CategorizedFile>& entries) {
                entries.erase(
                    std::remove_if(entries.begin(),
                                   entries.end(),
                                   [&](const CategorizedFile& entry) {
                                       if (entry.type != FileType::File) {
                                           return true;
                                       }
                                       const auto full_path = Utils::utf8_to_path(entry.file_path) /
                                                              Utils::utf8_to_path(entry.file_name);
                                       const bool is_image = LlavaImageAnalyzer::is_supported_image(full_path);
                                       const bool is_document = DocumentTextAnalyzer::is_supported_document(full_path);
                                       if (is_image && allow_images) {
                                           return false;
                                       }
                                       if (is_document && allow_documents) {
                                           return false;
                                       }
                                       return true;
                                   }),
                    entries.end());
            };
            filter_entries(app_.already_categorized_files);
            filter_entries(pending_renames);
            if (!cached_visual_indices.empty()) {
                for (size_t index = 0; index < app_.already_categorized_files.size(); ++index) {
                    auto it = cached_visual_indices.find(file_key(app_.already_categorized_files[index]));
                    if (it != cached_visual_indices.end()) {
                        it->second = index;
                    }
                }
            }
            if (!cached_document_indices.empty()) {
                for (size_t index = 0; index < app_.already_categorized_files.size(); ++index) {
                    auto it = cached_document_indices.find(file_key(app_.already_categorized_files[index]));
                    if (it != cached_document_indices.end()) {
                        it->second = index;
                    }
                }
            }
        }

        update_stop();

        app_.log_cached_highlights();

        auto cached_file_names =
            app_.results_coordinator.extract_file_names(app_.already_categorized_files, use_full_path_keys);
        if ((rename_images_only || rename_documents_only) && !pending_renames.empty()) {
            for (const auto& entry : pending_renames) {
                cached_file_names.insert(file_key(entry));
            }
        }
        const auto scan_options = app_.effective_scan_options();
        app_.files_to_categorize = app_.results_coordinator.find_files_to_categorize(directory_path,
                                                                                     scan_options,
                                                                                     cached_file_names,
                                                                                     use_full_path_keys);
        if (process_images_only || process_documents_only) {
            const bool allow_images = process_images_only;
            const bool allow_documents = process_documents_only;
            app_.files_to_categorize.erase(
                std::remove_if(app_.files_to_categorize.begin(),
                               app_.files_to_categorize.end(),
                               [&](const FileEntry& entry) {
                                   if (entry.type != FileType::File) {
                                       return true;
                                   }
                                   const bool is_image = LlavaImageAnalyzer::is_supported_image(entry.full_path);
                                   const bool is_document =
                                       DocumentTextAnalyzer::is_supported_document(entry.full_path);
                                   if (is_image && allow_images) {
                                       return false;
                                   }
                                   if (is_document && allow_documents) {
                                       return false;
                                   }
                                   return true;
                               }),
                app_.files_to_categorize.end());
        }
        app_.core_logger->debug("Found {} item(s) pending categorization in '{}'.",
                                app_.files_to_categorize.size(),
                                directory_path);

        app_.log_pending_queue();
        update_stop();

        app_.append_progress(to_utf8(app_.tr("[PROCESS] Letting the AI do its magic...")));

        std::vector<FileEntry> image_entries;
        std::vector<FileEntry> document_entries;
        std::vector<FileEntry> other_entries;
        AnalysisEntryRouter::split_entries_for_analysis(app_.files_to_categorize,
                                                        analyze_images,
                                                        analyze_documents,
                                                        process_images_only,
                                                        process_documents_only,
                                                        rename_images_only,
                                                        rename_documents_only,
                                                        app_.settings.get_categorize_files(),
                                                        use_full_path_keys,
                                                        renamed_files,
                                                        image_entries,
                                                        document_entries,
                                                        other_entries);
        if (!cached_image_entries_for_visual.empty()) {
            image_entries.insert(image_entries.end(),
                                 cached_image_entries_for_visual.begin(),
                                 cached_image_entries_for_visual.end());
        }
        if (!cached_document_entries_for_analysis.empty()) {
            document_entries.insert(document_entries.end(),
                                    cached_document_entries_for_analysis.begin(),
                                    cached_document_entries_for_analysis.end());
        }

        using ProgressStageId = CategorizationProgressDialog::StageId;

        std::vector<FileEntry> image_stage_entries;
        image_stage_entries.reserve(image_entries.size());
        for (const auto& entry : image_entries) {
            const bool already_renamed = renamed_files.contains(entry_key(entry));
            if (already_renamed && rename_images_only) {
                continue;
            }
            image_stage_entries.push_back(entry);
        }

        std::vector<FileEntry> document_stage_entries;
        document_stage_entries.reserve(document_entries.size());
        for (const auto& entry : document_entries) {
            const bool already_renamed = renamed_files.contains(entry_key(entry));
            if (already_renamed && rename_documents_only) {
                continue;
            }
            document_stage_entries.push_back(entry);
        }

        std::vector<FileEntry> planned_categorization_entries;
        std::unordered_set<std::string> planned_categorization_seen;
        planned_categorization_entries.reserve(other_entries.size() +
                                               image_entries.size() +
                                               document_entries.size());
        auto append_planned_categorization_entry = [&](const FileEntry& entry) {
            const std::string key = entry_key(entry);
            if (planned_categorization_seen.contains(key)) {
                return;
            }
            planned_categorization_seen.insert(key);
            planned_categorization_entries.push_back(entry);
        };

        for (const auto& entry : other_entries) {
            append_planned_categorization_entry(entry);
        }
        if (!rename_images_only) {
            for (const auto& entry : image_entries) {
                if (cached_visual_indices.contains(entry_key(entry))) {
                    continue;
                }
                append_planned_categorization_entry(entry);
            }
        }
        if (!rename_documents_only) {
            for (const auto& entry : document_entries) {
                if (cached_document_indices.contains(entry_key(entry))) {
                    continue;
                }
                append_planned_categorization_entry(entry);
            }
        }

        std::vector<CategorizationProgressDialog::StagePlan> progress_stages;
        if (!image_stage_entries.empty()) {
            progress_stages.push_back({ProgressStageId::ImageAnalysis, image_stage_entries});
        }
        if (!document_stage_entries.empty()) {
            progress_stages.push_back({ProgressStageId::DocumentAnalysis, document_stage_entries});
        }
        if (!planned_categorization_entries.empty()) {
            progress_stages.push_back({ProgressStageId::Categorization, planned_categorization_entries});
        }

        app_.configure_progress_stages(progress_stages);
        if (!progress_stages.empty()) {
            app_.set_progress_active_stage(progress_stages.front().id);
        }

        struct ImageAnalysisInfo {
            std::string suggested_name;
            std::string prompt_name;
            std::string prompt_path;
        };

        struct DocumentAnalysisInfo {
            std::string suggested_name;
            std::string prompt_name;
            std::string prompt_path;
        };

        std::unordered_map<std::string, ImageAnalysisInfo> image_info;
        std::vector<FileEntry> image_entries_for_llm;
        image_entries_for_llm.reserve(image_entries.size());
        std::vector<FileEntry> analyzed_image_entries;
        analyzed_image_entries.reserve(image_entries.size());

        std::unordered_map<std::string, DocumentAnalysisInfo> document_info;
        std::unordered_map<std::string, std::string> image_dates;
        std::unordered_map<std::string, std::string> document_dates;
        std::vector<FileEntry> document_entries_for_llm;
        document_entries_for_llm.reserve(document_entries.size());
        std::vector<FileEntry> analyzed_document_entries;
        analyzed_document_entries.reserve(document_entries.size());
        std::unique_ptr<ImageRenameMetadataService> image_metadata_service;
        std::unique_ptr<MediaRenameMetadataService> media_metadata_service;
        std::unordered_map<std::string, std::string> media_rename_suggestions;
        if (add_image_date_place_prefixes || add_image_date_to_category) {
            image_metadata_service =
                std::make_unique<ImageRenameMetadataService>(app_.settings.get_config_dir());
        }
        if (add_audio_video_metadata_to_filename) {
            media_metadata_service = std::make_unique<MediaRenameMetadataService>();
        }

        if (analyze_images && !image_entries.empty()) {
            if (!image_stage_entries.empty()) {
                app_.set_progress_active_stage(ProgressStageId::ImageAnalysis);
            }
            auto enrich_image_suggestion = [&](const FileEntry& entry,
                                               const std::string& raw_suggested_name) {
                if (raw_suggested_name.empty() || !image_metadata_service) {
                    return raw_suggested_name;
                }
                return image_metadata_service->enrich_suggested_name(Utils::utf8_to_path(entry.full_path),
                                                                     raw_suggested_name);
            };
            auto cache_image_date = [&](const FileEntry& entry) {
                if (!add_image_date_to_category || !image_metadata_service) {
                    return;
                }
                const std::string key = entry_key(entry);
                if (image_dates.contains(key)) {
                    return;
                }
                if (const auto date =
                        image_metadata_service->extract_capture_date(Utils::utf8_to_path(entry.full_path))) {
                    image_dates.emplace(key, *date);
                }
            };

            std::string error;
            auto visual_paths = VisualLlmRuntime::resolve_paths(&error);
            if (!visual_paths) {
                throw std::runtime_error(error);
            }

            LlavaImageAnalyzer::Settings vision_settings;
            vision_settings.use_gpu = VisualLlmRuntime::should_use_gpu();
            const auto visual_gpu_override = read_env_bool("AI_FILE_SORTER_VISUAL_USE_GPU");
            if (visual_gpu_override.has_value()) {
                vision_settings.use_gpu = *visual_gpu_override;
            }
            vision_settings.batch_progress = [this](int current_batch, int total_batches) {
                if (total_batches <= 0 || current_batch <= 0) {
                    return;
                }
                const double percent =
                    (static_cast<double>(current_batch) / static_cast<double>(total_batches)) * 100.0;
                app_.append_progress(to_utf8(app_.tr("[VISION] Decoding image batch %1/%2 (%3%)")
                                                 .arg(current_batch)
                                                 .arg(total_batches)
                                                 .arg(percent, 0, 'f', 2)));
            };
            vision_settings.log_visual_output = app_.should_log_prompts();

            const bool allow_visual_cpu_fallback =
                vision_settings.use_gpu && !visual_gpu_override.has_value();
            std::optional<bool> visual_cpu_fallback_choice;
            bool visual_cpu_fallback_active = false;

            auto should_retry_on_cpu = [](const std::exception& ex) {
                return VisualLlmRuntime::should_offer_cpu_fallback(ex.what());
            };

            auto prompt_visual_cpu_fallback = [this]() -> bool {
                auto show_dialog = [this]() -> bool {
                    QMessageBox box(&app_);
                    box.setIcon(QMessageBox::Question);
                    box.setWindowTitle(app_.tr("Switch image analysis to CPU?"));
                    box.setText(app_.tr("Image analysis ran out of GPU memory."));
                    box.setInformativeText(
                        app_.tr("Retry on CPU instead? Cancel will skip visual analysis and fall back to "
                                "filename-based categorization."));
                    box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
                    box.setDefaultButton(QMessageBox::Ok);
                    return box.exec() == QMessageBox::Ok;
                };
                if (QThread::currentThread() == app_.thread()) {
                    return show_dialog();
                }
                bool decision = false;
                QMetaObject::invokeMethod(
                    &app_,
                    [&decision, show_dialog]() mutable { decision = show_dialog(); },
                    Qt::BlockingQueuedConnection);
                return decision;
            };

            auto update_cached_image_suggestion = [&](const FileEntry& entry,
                                                      const std::string& suggested_name) {
                const auto it = cached_visual_indices.find(entry_key(entry));
                if (it == cached_visual_indices.end()) {
                    return;
                }
                auto& cached_entry = app_.already_categorized_files[it->second];
                if (cached_entry.suggested_name == suggested_name) {
                    return;
                }
                cached_entry.suggested_name = suggested_name;
                persist_cached_suggestion(cached_entry, suggested_name);
            };

            auto handle_visual_failure = [&](const FileEntry& entry,
                                             const std::string& reason,
                                             bool already_renamed,
                                             bool log_failure,
                                             bool visual_only) {
                if (log_failure) {
                    app_.append_progress(to_utf8(app_.tr("[VISION-ERROR] %1 (%2)")
                                                     .arg(QString::fromStdString(entry.file_name),
                                                          QString::fromStdString(reason))));
                }
                if (!rename_images_only && !visual_only) {
                    other_entries.push_back(entry);
                }
                const std::string suggested_name =
                    already_renamed ? std::string() : enrich_image_suggestion(entry, entry.file_name);
                const std::string ui_suggested_name =
                    (allow_image_renames || rename_images_only) ? suggested_name : std::string();
                image_info.emplace(entry_key(entry),
                                   ImageAnalysisInfo{ui_suggested_name, entry.file_name, entry.full_path});
                if (rename_images_only) {
                    persist_rename_only_progress(entry, suggested_name);
                }
                if (visual_only) {
                    update_cached_image_suggestion(entry, suggested_name);
                }
            };

            auto create_analyzer = [&]() -> std::unique_ptr<LlavaImageAnalyzer> {
                return std::make_unique<LlavaImageAnalyzer>(visual_paths->model_path,
                                                            visual_paths->mmproj_path,
                                                            vision_settings);
            };

            std::unique_ptr<LlavaImageAnalyzer> analyzer;
            bool skip_visual_analysis = false;
            std::string skip_visual_reason;
            try {
                analyzer = create_analyzer();
            } catch (const std::exception& ex) {
                const bool retry_on_cpu = should_retry_on_cpu(ex);
                if (app_.core_logger) {
                    app_.core_logger->warn(
                        "Visual analyzer initialization failed (retryable_on_cpu={}): {}",
                        retry_on_cpu,
                        ex.what());
                }
                if (!allow_visual_cpu_fallback) {
                    throw;
                }
                if (!retry_on_cpu) {
                    skip_visual_analysis = true;
                    skip_visual_reason = ex.what();
                    if (app_.core_logger) {
                        app_.core_logger->warn(
                            "Visual analysis disabled after non-retryable initialization failure.");
                    }
                } else {
                    if (!visual_cpu_fallback_choice.has_value()) {
                        visual_cpu_fallback_choice = prompt_visual_cpu_fallback();
                    }
                    if (!visual_cpu_fallback_choice.value()) {
                        skip_visual_analysis = true;
                        skip_visual_reason = ex.what();
                        if (app_.core_logger) {
                            app_.core_logger->warn(
                                "Visual CPU fallback declined after initialization failure: {}",
                                ex.what());
                        }
                    } else {
                        app_.append_progress(
                            to_utf8(app_.tr("[VISION] Switching visual analysis to CPU.")));
                        vision_settings.use_gpu = false;
                        visual_cpu_fallback_active = true;
                        if (app_.core_logger) {
                            app_.core_logger->warn(
                                "Retrying visual analyzer initialization on CPU after GPU failure: {}",
                                ex.what());
                        }
                        try {
                            analyzer = create_analyzer();
                            if (app_.core_logger) {
                                app_.core_logger->info(
                                    "Visual analyzer CPU fallback initialized successfully.");
                            }
                        } catch (const std::exception& init_ex) {
                            skip_visual_analysis = true;
                            skip_visual_reason = init_ex.what();
                            if (app_.core_logger) {
                                app_.core_logger->error(
                                    "Visual analyzer CPU fallback initialization failed: {}",
                                    init_ex.what());
                            }
                        }
                    }
                }
            }

            if (skip_visual_analysis) {
                if (!skip_visual_reason.empty()) {
                    if (app_.core_logger) {
                        app_.core_logger->warn(
                            "Visual analysis disabled; falling back to filenames: {}",
                            skip_visual_reason);
                    }
                    app_.append_progress(
                        to_utf8(app_.tr("[VISION-ERROR] %1")
                                    .arg(QString::fromStdString(skip_visual_reason))));
                }
                app_.append_progress(to_utf8(
                    app_.tr("[VISION] Visual analysis disabled; falling back to filenames.")));
                for (const auto& entry : image_entries) {
                    if (update_stop()) {
                        break;
                    }
                    const bool already_renamed = renamed_files.contains(entry_key(entry));
                    if (already_renamed && rename_images_only) {
                        continue;
                    }
                    const bool visual_only = cached_visual_indices.contains(entry_key(entry));
                    analyzed_image_entries.push_back(entry);
                    cache_image_date(entry);
                    app_.mark_progress_stage_item_in_progress(ProgressStageId::ImageAnalysis, entry);
                    handle_visual_failure(entry, std::string(), already_renamed, false, visual_only);
                    app_.mark_progress_stage_item_completed(ProgressStageId::ImageAnalysis, entry);
                }
            } else {
                bool stop_visual_analysis = false;
                for (size_t index = 0; index < image_entries.size(); ++index) {
                    const auto& entry = image_entries[index];
                    if (update_stop()) {
                        break;
                    }
                    const bool already_renamed = renamed_files.contains(entry_key(entry));
                    if (already_renamed && rename_images_only) {
                        continue;
                    }
                    const bool visual_only = cached_visual_indices.contains(entry_key(entry));
                    const auto cached_suggestion_it = cached_image_suggestions.find(entry_key(entry));
                    const bool has_cached_suggestion = cached_suggestion_it != cached_image_suggestions.end();
                    analyzed_image_entries.push_back(entry);
                    cache_image_date(entry);
                    app_.mark_progress_stage_item_in_progress(ProgressStageId::ImageAnalysis, entry);

                    while (true) {
                        try {
                            if (has_cached_suggestion) {
                                app_.append_progress(to_utf8(app_.tr("[VISION] Using cached suggestion for %1")
                                                                 .arg(QString::fromStdString(entry.file_name))));
                                const std::string prompt_name = cached_suggestion_it->second;
                                const std::string enriched_name = enrich_image_suggestion(entry, prompt_name);
                                const std::string suggested_name =
                                    already_renamed ? std::string() : enriched_name;
                                const std::string ui_suggested_name =
                                    (allow_image_renames || rename_images_only) ? suggested_name : std::string();
                                const auto entry_path = Utils::utf8_to_path(entry.full_path);
                                const auto prompt_path = Utils::path_to_utf8(
                                    entry_path.parent_path() / Utils::utf8_to_path(prompt_name));
                                image_info.emplace(entry_key(entry),
                                                   ImageAnalysisInfo{ui_suggested_name,
                                                                     prompt_name,
                                                                     prompt_path});
                                if (rename_images_only) {
                                    persist_rename_only_progress(entry, suggested_name);
                                }
                                if (visual_only) {
                                    update_cached_image_suggestion(entry, suggested_name);
                                }
                                if (!rename_images_only && !visual_only) {
                                    image_entries_for_llm.push_back(entry);
                                }
                                app_.mark_progress_stage_item_completed(ProgressStageId::ImageAnalysis,
                                                                        entry);
                                break;
                            }

                            app_.append_progress(to_utf8(
                                app_.tr("[VISION] Analyzing %1")
                                    .arg(QString::fromStdString(entry.file_name))));
                            const auto analysis = analyzer->analyze(entry.full_path);
                            const std::string prompt_name = analysis.suggested_name;
                            const std::string enriched_name = enrich_image_suggestion(entry, prompt_name);
                            const auto entry_path = Utils::utf8_to_path(entry.full_path);
                            const auto prompt_path = Utils::path_to_utf8(
                                entry_path.parent_path() / Utils::utf8_to_path(prompt_name));

                            const std::string suggested_name =
                                already_renamed ? std::string() : enriched_name;
                            const std::string ui_suggested_name =
                                (allow_image_renames || rename_images_only) ? suggested_name : std::string();
                            if (!rename_images_only) {
                                persist_llm_suggestion_progress(entry, suggested_name);
                            }
                            image_info.emplace(entry_key(entry),
                                               ImageAnalysisInfo{ui_suggested_name,
                                                                 prompt_name,
                                                                 prompt_path});
                            if (rename_images_only) {
                                persist_rename_only_progress(entry, suggested_name);
                            }
                            if (visual_only) {
                                update_cached_image_suggestion(entry, suggested_name);
                            }

                            if (!rename_images_only && !visual_only) {
                                image_entries_for_llm.push_back(entry);
                            }
                            app_.mark_progress_stage_item_completed(ProgressStageId::ImageAnalysis, entry);
                            break;
                        } catch (const std::exception& ex) {
                            const bool retry_on_cpu = should_retry_on_cpu(ex);
                            if (!visual_cpu_fallback_active &&
                                allow_visual_cpu_fallback &&
                                retry_on_cpu) {
                                if (app_.core_logger) {
                                    app_.core_logger->warn(
                                        "Visual analysis failed for '{}' with retryable GPU error: {}",
                                        entry.file_name,
                                        ex.what());
                                }
                                if (!visual_cpu_fallback_choice.has_value()) {
                                    visual_cpu_fallback_choice = prompt_visual_cpu_fallback();
                                }
                                if (visual_cpu_fallback_choice.value()) {
                                    app_.append_progress(to_utf8(
                                        app_.tr("[VISION] GPU memory issue detected. Switching to CPU.")));
                                    vision_settings.use_gpu = false;
                                    visual_cpu_fallback_active = true;
                                    if (app_.core_logger) {
                                        app_.core_logger->warn(
                                            "Retrying visual analysis on CPU for '{}'.",
                                            entry.file_name);
                                    }
                                    try {
                                        analyzer = create_analyzer();
                                        if (app_.core_logger) {
                                            app_.core_logger->info(
                                                "Visual analyzer CPU fallback initialized successfully; retrying '{}'.",
                                                entry.file_name);
                                        }
                                    } catch (const std::exception& init_ex) {
                                        if (app_.core_logger) {
                                            app_.core_logger->error(
                                                "Visual analyzer CPU fallback initialization failed for '{}': {}",
                                                entry.file_name,
                                                init_ex.what());
                                        }
                                        handle_visual_failure(entry,
                                                              init_ex.what(),
                                                              already_renamed,
                                                              true,
                                                              visual_only);
                                        app_.append_progress(to_utf8(app_.tr(
                                            "[VISION] Visual analysis disabled for remaining images.")));
                                        stop_visual_analysis = true;
                                    }
                                    if (!stop_visual_analysis) {
                                        continue;
                                    }
                                } else {
                                    if (app_.core_logger) {
                                        app_.core_logger->warn(
                                            "Visual CPU fallback declined for '{}': {}",
                                            entry.file_name,
                                            ex.what());
                                    }
                                    app_.append_progress(to_utf8(app_.tr(
                                        "[VISION] Visual analysis disabled; falling back to filenames.")));
                                    handle_visual_failure(entry, ex.what(), already_renamed, true, visual_only);
                                    stop_visual_analysis = true;
                                }
                            } else {
                                if (app_.core_logger) {
                                    app_.core_logger->warn("Visual analysis failed for '{}': {}",
                                                           entry.file_name,
                                                           ex.what());
                                }
                                handle_visual_failure(entry, ex.what(), already_renamed, true, visual_only);
                            }
                            app_.mark_progress_stage_item_completed(ProgressStageId::ImageAnalysis, entry);
                            break;
                        }
                    }

                    if (stop_visual_analysis) {
                        for (size_t remaining = index + 1; remaining < image_entries.size(); ++remaining) {
                            if (update_stop()) {
                                break;
                            }
                            const auto& pending = image_entries[remaining];
                            const bool pending_renamed = renamed_files.contains(entry_key(pending));
                            if (pending_renamed && rename_images_only) {
                                continue;
                            }
                            const bool pending_visual_only =
                                cached_visual_indices.contains(entry_key(pending));
                            analyzed_image_entries.push_back(pending);
                            cache_image_date(pending);
                            app_.mark_progress_stage_item_in_progress(ProgressStageId::ImageAnalysis,
                                                                      pending);
                            handle_visual_failure(pending,
                                                  std::string(),
                                                  pending_renamed,
                                                  false,
                                                  pending_visual_only);
                            app_.mark_progress_stage_item_completed(ProgressStageId::ImageAnalysis,
                                                                    pending);
                        }
                        break;
                    }
                }
            }
        }

        if (analyze_documents && !document_entries.empty()) {
            if (!document_stage_entries.empty()) {
                app_.set_progress_active_stage(ProgressStageId::DocumentAnalysis);
            }
            auto update_cached_document_suggestion = [&](const FileEntry& entry,
                                                         const std::string& suggested_name) {
                const auto it = cached_document_indices.find(entry_key(entry));
                if (it == cached_document_indices.end()) {
                    return;
                }
                auto& cached_entry = app_.already_categorized_files[it->second];
                if (cached_entry.suggested_name == suggested_name) {
                    return;
                }
                cached_entry.suggested_name = suggested_name;
                persist_cached_suggestion(cached_entry, suggested_name);
            };

            auto handle_document_failure = [&](const FileEntry& entry,
                                               const std::string& reason,
                                               bool already_renamed,
                                               bool log_failure,
                                               bool document_only) {
                if (log_failure) {
                    app_.append_progress(to_utf8(app_.tr("[DOC-ERROR] %1 (%2)")
                                                     .arg(QString::fromStdString(entry.file_name),
                                                          QString::fromStdString(reason))));
                    if (app_.core_logger) {
                        const char* fallback_action = "falling back to filename-based categorization";
                        if (rename_documents_only) {
                            fallback_action = "keeping the original filename in rename-only mode";
                        } else if (document_only) {
                            fallback_action = "restoring the original filename as the cached suggestion";
                        }
                        app_.core_logger->warn("Document analysis failed for '{}': {}; {}.",
                                               entry.file_name,
                                               reason,
                                               fallback_action);
                    }
                }
                if (!rename_documents_only && !document_only) {
                    other_entries.push_back(entry);
                }
                const std::string suggested_name = already_renamed ? std::string() : entry.file_name;
                const std::string ui_suggested_name =
                    (allow_document_renames || rename_documents_only) ? suggested_name : std::string();
                const std::string prompt_name = entry.file_name;
                document_info.emplace(
                    entry_key(entry),
                    DocumentAnalysisInfo{ui_suggested_name,
                                         prompt_name,
                                         build_document_prompt_path(entry.full_path, prompt_name, {})});
                if (rename_documents_only) {
                    persist_rename_only_progress(entry, suggested_name);
                }
                if (document_only) {
                    update_cached_document_suggestion(entry, suggested_name);
                }
            };

            DocumentTextAnalyzer::Settings doc_settings;
            doc_settings.max_tokens = 256;
            const size_t char_budget =
                resolve_document_char_budget(app_.using_local_llm, doc_settings.max_tokens);
            doc_settings.max_characters = std::min(doc_settings.max_characters, char_budget);
            DocumentTextAnalyzer doc_analyzer(doc_settings);

            auto llm = app_.make_llm_client();
            if (!llm) {
                throw std::runtime_error("Failed to create LLM client.");
            }
            llm->set_prompt_logging_enabled(app_.should_log_prompts());

            for (const auto& entry : document_entries) {
                if (update_stop()) {
                    break;
                }
                const bool already_renamed = renamed_files.contains(entry_key(entry));
                if (already_renamed && rename_documents_only) {
                    continue;
                }
                const bool document_only = cached_document_indices.contains(entry_key(entry));
                const auto cached_suggestion_it = cached_document_suggestions.find(entry_key(entry));
                const bool has_cached_suggestion = cached_suggestion_it != cached_document_suggestions.end();
                if (add_document_date && !document_dates.contains(entry_key(entry))) {
                    const auto date =
                        DocumentTextAnalyzer::extract_creation_date(Utils::utf8_to_path(entry.full_path));
                    if (date) {
                        document_dates.emplace(entry_key(entry), *date);
                    }
                }
                analyzed_document_entries.push_back(entry);
                app_.mark_progress_stage_item_in_progress(ProgressStageId::DocumentAnalysis, entry);

                try {
                    if (has_cached_suggestion) {
                        app_.append_progress(to_utf8(app_.tr("[DOC] Using cached suggestion for %1")
                                                         .arg(QString::fromStdString(entry.file_name))));
                        const std::string suggested_name =
                            already_renamed ? std::string() : cached_suggestion_it->second;
                        const std::string ui_suggested_name =
                            (allow_document_renames || rename_documents_only)
                                ? suggested_name
                                : std::string();
                        const std::string prompt_name =
                            resolve_document_prompt_name(entry.file_name, cached_suggestion_it->second);
                        document_info.emplace(entry_key(entry),
                                              DocumentAnalysisInfo{
                                                  ui_suggested_name,
                                                  prompt_name,
                                                  build_document_prompt_path(entry.full_path, prompt_name, {})});
                        if (rename_documents_only) {
                            persist_rename_only_progress(entry, suggested_name);
                        }
                        if (!rename_documents_only && !document_only) {
                            document_entries_for_llm.push_back(entry);
                        }
                        app_.mark_progress_stage_item_completed(ProgressStageId::DocumentAnalysis,
                                                                entry);
                        continue;
                    }

                    app_.append_progress(to_utf8(
                        app_.tr("[DOC] Analyzing %1").arg(QString::fromStdString(entry.file_name))));
                    const auto analysis = doc_analyzer.analyze(Utils::utf8_to_path(entry.full_path), *llm);
                    const std::string suggested_name =
                        already_renamed ? std::string() : analysis.suggested_name;
                    const std::string ui_suggested_name =
                        (allow_document_renames || rename_documents_only)
                            ? suggested_name
                            : std::string();
                    const std::string prompt_name =
                        resolve_document_prompt_name(entry.file_name, analysis.suggested_name);
                    const std::string prompt_path =
                        build_document_prompt_path(entry.full_path, prompt_name, analysis.summary);
                    if (!rename_documents_only) {
                        persist_llm_suggestion_progress(entry, suggested_name);
                    }
                    document_info.emplace(
                        entry_key(entry),
                        DocumentAnalysisInfo{ui_suggested_name, prompt_name, prompt_path});
                    if (rename_documents_only) {
                        persist_rename_only_progress(entry, suggested_name);
                    }
                    if (document_only) {
                        update_cached_document_suggestion(entry, suggested_name);
                    }
                    if (!rename_documents_only && !document_only) {
                        document_entries_for_llm.push_back(entry);
                    }
                    app_.mark_progress_stage_item_completed(ProgressStageId::DocumentAnalysis, entry);
                } catch (const std::exception& ex) {
                    handle_document_failure(entry, ex.what(), already_renamed, true, document_only);
                    app_.mark_progress_stage_item_completed(ProgressStageId::DocumentAnalysis, entry);
                }
            }
        }

        update_stop();

        std::vector<FileEntry> categorization_stage_entries;
        std::unordered_set<std::string> categorization_stage_seen;
        categorization_stage_entries.reserve(other_entries.size() +
                                             image_entries_for_llm.size() +
                                             document_entries_for_llm.size());
        auto append_categorization_stage_entry = [&](const FileEntry& entry) {
            const std::string key = entry_key(entry);
            if (categorization_stage_seen.contains(key)) {
                return;
            }
            categorization_stage_seen.insert(key);
            categorization_stage_entries.push_back(entry);
        };

        if (!stop_requested) {
            for (const auto& entry : other_entries) {
                append_categorization_stage_entry(entry);
            }
        }
        for (const auto& entry : image_entries_for_llm) {
            append_categorization_stage_entry(entry);
        }
        for (const auto& entry : document_entries_for_llm) {
            append_categorization_stage_entry(entry);
        }

        app_.set_progress_stage_items(ProgressStageId::Categorization, categorization_stage_entries);
        if (!categorization_stage_entries.empty()) {
            app_.set_progress_active_stage(ProgressStageId::Categorization);
        }

        auto suggested_name_provider = [allow_image_renames,
                                        allow_document_renames,
                                        add_audio_video_metadata_to_filename,
                                        &image_info,
                                        &document_info,
                                        &media_metadata_service,
                                        &media_rename_suggestions,
                                        &entry_key](const FileEntry& entry) -> std::string {
            const std::string key = entry_key(entry);
            if (allow_image_renames) {
                if (const auto it = image_info.find(key); it != image_info.end()) {
                    return it->second.suggested_name;
                }
            }
            if (allow_document_renames) {
                if (const auto it = document_info.find(key); it != document_info.end()) {
                    return it->second.suggested_name;
                }
            }
            if (add_audio_video_metadata_to_filename &&
                media_metadata_service &&
                entry.type == FileType::File) {
                const auto cache_it = media_rename_suggestions.find(key);
                if (cache_it != media_rename_suggestions.end()) {
                    return cache_it->second;
                }
                std::string suggestion;
                if (MediaRenameMetadataService::is_supported_media(Utils::utf8_to_path(entry.full_path))) {
                    if (const auto suggested =
                            media_metadata_service->suggest_name(Utils::utf8_to_path(entry.full_path))) {
                        suggestion = *suggested;
                    }
                }
                media_rename_suggestions.emplace(key, suggestion);
                return suggestion;
            }
            return std::string();
        };

        auto apply_image_dates =
            [this, add_image_date_to_category, &image_dates, &file_key, &resolve_entry_for_storage, &image_metadata_service](
                std::vector<CategorizedFile>& results) {
                if (!add_image_date_to_category) {
                    return;
                }
                for (auto& entry : results) {
                    if (entry.type != FileType::File) {
                        continue;
                    }
                    const auto full_path = Utils::utf8_to_path(entry.file_path) /
                                           Utils::utf8_to_path(entry.file_name);
                    if (!LlavaImageAnalyzer::is_supported_image(full_path)) {
                        continue;
                    }
                    const std::string key = file_key(entry);
                    auto it = image_dates.find(key);
                    if (it == image_dates.end() && image_metadata_service) {
                        if (const auto date = image_metadata_service->extract_capture_date(full_path)) {
                            it = image_dates.emplace(key, *date).first;
                        }
                    }
                    if (it == image_dates.end() || it->second.empty()) {
                        continue;
                    }
                    if (entry.category.empty()) {
                        continue;
                    }
                    const std::string suffix = "_" + it->second;
                    if (entry.category.size() >= suffix.size() &&
                        entry.category.compare(entry.category.size() - suffix.size(),
                                               suffix.size(),
                                               suffix) == 0) {
                        continue;
                    }
                    entry.category += suffix;
                    if (entry.canonical_category.empty()) {
                        entry.canonical_category =
                            entry.category.substr(0, entry.category.size() - suffix.size());
                    }
                    entry.canonical_category += suffix;

                    DatabaseManager::ResolvedCategory resolved = resolve_entry_for_storage(entry);
                    const std::string file_type_label =
                        (entry.type == FileType::Directory) ? "D" : "F";
                    app_.db_manager.insert_or_update_file_with_categorization(entry.file_name,
                                                                              file_type_label,
                                                                              entry.file_path,
                                                                              resolved,
                                                                              entry.used_consistency_hints,
                                                                              entry.suggested_name,
                                                                              entry.rename_only,
                                                                              entry.rename_applied);
                }
            };

        auto apply_document_dates =
            [this, add_document_date, &document_dates, &file_key, &resolve_entry_for_storage](
                std::vector<CategorizedFile>& results) {
                if (!add_document_date || document_dates.empty()) {
                    return;
                }
                for (auto& entry : results) {
                    if (entry.type != FileType::File) {
                        continue;
                    }
                    const auto full_path = Utils::utf8_to_path(entry.file_path) /
                                           Utils::utf8_to_path(entry.file_name);
                    if (!DocumentTextAnalyzer::is_supported_document(full_path)) {
                        continue;
                    }
                    const std::string key = file_key(entry);
                    auto it = document_dates.find(key);
                    if (it == document_dates.end()) {
                        if (const auto date = DocumentTextAnalyzer::extract_creation_date(full_path)) {
                            it = document_dates.emplace(key, *date).first;
                        }
                    }
                    if (it == document_dates.end() || it->second.empty()) {
                        continue;
                    }
                    if (entry.category.empty()) {
                        continue;
                    }
                    const std::string suffix = "_" + it->second;
                    if (entry.category.size() >= suffix.size() &&
                        entry.category.compare(entry.category.size() - suffix.size(),
                                               suffix.size(),
                                               suffix) == 0) {
                        continue;
                    }
                    entry.category += suffix;
                    if (entry.canonical_category.empty()) {
                        entry.canonical_category =
                            entry.category.substr(0, entry.category.size() - suffix.size());
                    }
                    entry.canonical_category += suffix;

                    DatabaseManager::ResolvedCategory resolved = resolve_entry_for_storage(entry);
                    const std::string file_type_label =
                        (entry.type == FileType::Directory) ? "D" : "F";
                    app_.db_manager.insert_or_update_file_with_categorization(entry.file_name,
                                                                              file_type_label,
                                                                              entry.file_path,
                                                                              resolved,
                                                                              entry.used_consistency_hints,
                                                                              entry.suggested_name,
                                                                              entry.rename_only,
                                                                              entry.rename_applied);
                }
            };

        std::vector<CategorizedFile> other_results;
        if (!stop_requested && !other_entries.empty()) {
            other_results = app_.categorization_service.categorize_entries(
                other_entries,
                app_.using_local_llm,
                app_.stop_analysis,
                [this](const std::string& message) { app_.append_progress(message); },
                [this](const FileEntry& entry) {
                    app_.mark_progress_stage_item_in_progress(ProgressStageId::Categorization, entry);
                    const QString type_label =
                        entry.type == FileType::Directory ? app_.tr("Directory") : app_.tr("File");
                    app_.append_progress(to_utf8(app_.tr("[SORT] %1 (%2)")
                                                     .arg(QString::fromStdString(entry.file_name), type_label)));
                },
                [this](const FileEntry& entry) {
                    app_.mark_progress_stage_item_completed(ProgressStageId::Categorization, entry);
                },
                [this](const CategorizedFile& entry, const std::string& reason) {
                    app_.notify_recategorization_reset(entry, reason);
                },
                [this]() { return app_.make_llm_client(); },
                {},
                suggested_name_provider);
        }
        apply_image_dates(other_results);
        apply_document_dates(other_results);
        update_stop();

        std::vector<CategorizedFile> image_results;
        if (analyze_images && !analyzed_image_entries.empty()) {
            if (rename_images_only) {
                image_results.reserve(analyzed_image_entries.size());
                for (const auto& entry : analyzed_image_entries) {
                    const auto entry_path = Utils::utf8_to_path(entry.full_path);
                    CategorizedFile result{Utils::path_to_utf8(entry_path.parent_path()),
                                           entry.file_name,
                                           entry.type,
                                           "",
                                           "",
                                           0};
                    result.rename_only = true;
                    if (offer_image_renames || rename_images_only) {
                        const auto info_it = image_info.find(entry_key(entry));
                        if (info_it != image_info.end()) {
                            result.suggested_name = info_it->second.suggested_name;
                        }
                    }
                    image_results.push_back(std::move(result));
                }
            } else if (!image_entries_for_llm.empty()) {
                const bool stop_before_image_categorization = stop_requested;
                const bool bypass_stop = stop_before_image_categorization && other_results.empty();
                std::atomic<bool> image_stop{false};
                std::atomic<bool>& stop_flag = bypass_stop ? image_stop : app_.stop_analysis;

                auto override_provider = [&image_info, &entry_key](const FileEntry& entry)
                    -> std::optional<CategorizationService::PromptOverride> {
                    const auto it = image_info.find(entry_key(entry));
                    if (it == image_info.end()) {
                        return std::nullopt;
                    }
                    return CategorizationService::PromptOverride{it->second.prompt_name,
                                                                 it->second.prompt_path};
                };

                image_results = app_.categorization_service.categorize_entries(
                    image_entries_for_llm,
                    app_.using_local_llm,
                    stop_flag,
                    [this](const std::string& message) { app_.append_progress(message); },
                    [this](const FileEntry& entry) {
                        app_.mark_progress_stage_item_in_progress(ProgressStageId::Categorization, entry);
                        const QString type_label =
                            entry.type == FileType::Directory ? app_.tr("Directory") : app_.tr("File");
                        app_.append_progress(
                            to_utf8(app_.tr("[SORT] %1 (%2)")
                                        .arg(QString::fromStdString(entry.file_name), type_label)));
                    },
                    [this](const FileEntry& entry) {
                        app_.mark_progress_stage_item_completed(ProgressStageId::Categorization, entry);
                    },
                    [this](const CategorizedFile& entry, const std::string& reason) {
                        app_.notify_recategorization_reset(entry, reason);
                    },
                    [this]() { return app_.make_llm_client(); },
                    override_provider,
                    suggested_name_provider);

                update_stop();
            }
        }
        apply_image_dates(image_results);

        std::vector<CategorizedFile> document_results;
        if (analyze_documents && !analyzed_document_entries.empty()) {
            if (rename_documents_only) {
                document_results.reserve(analyzed_document_entries.size());
                for (const auto& entry : analyzed_document_entries) {
                    const auto entry_path = Utils::utf8_to_path(entry.full_path);
                    CategorizedFile result{Utils::path_to_utf8(entry_path.parent_path()),
                                           entry.file_name,
                                           entry.type,
                                           "",
                                           "",
                                           0};
                    result.rename_only = true;
                    if (offer_document_renames || rename_documents_only) {
                        const auto info_it = document_info.find(entry_key(entry));
                        if (info_it != document_info.end()) {
                            result.suggested_name = info_it->second.suggested_name;
                        }
                    }
                    document_results.push_back(std::move(result));
                }
            } else if (!document_entries_for_llm.empty()) {
                const bool stop_before_doc_categorization = stop_requested;
                const bool bypass_stop =
                    stop_before_doc_categorization && other_results.empty() && image_results.empty();
                std::atomic<bool> doc_stop{false};
                std::atomic<bool>& stop_flag = bypass_stop ? doc_stop : app_.stop_analysis;

                auto override_provider = [&document_info, &entry_key](const FileEntry& entry)
                    -> std::optional<CategorizationService::PromptOverride> {
                    const auto it = document_info.find(entry_key(entry));
                    if (it == document_info.end()) {
                        return std::nullopt;
                    }
                    return CategorizationService::PromptOverride{it->second.prompt_name,
                                                                 it->second.prompt_path};
                };

                document_results = app_.categorization_service.categorize_entries(
                    document_entries_for_llm,
                    app_.using_local_llm,
                    stop_flag,
                    [this](const std::string& message) { app_.append_progress(message); },
                    [this](const FileEntry& entry) {
                        app_.mark_progress_stage_item_in_progress(ProgressStageId::Categorization, entry);
                        const QString type_label =
                            entry.type == FileType::Directory ? app_.tr("Directory") : app_.tr("File");
                        app_.append_progress(
                            to_utf8(app_.tr("[SORT] %1 (%2)")
                                        .arg(QString::fromStdString(entry.file_name), type_label)));
                    },
                    [this](const FileEntry& entry) {
                        app_.mark_progress_stage_item_completed(ProgressStageId::Categorization, entry);
                    },
                    [this](const CategorizedFile& entry, const std::string& reason) {
                        app_.notify_recategorization_reset(entry, reason);
                    },
                    [this]() { return app_.make_llm_client(); },
                    override_provider,
                    suggested_name_provider);
            }
        }

        apply_document_dates(document_results);

        update_stop();

        app_.new_files_with_categories.clear();
        app_.new_files_with_categories.reserve(other_results.size() + image_results.size() +
                                              document_results.size());
        app_.new_files_with_categories.insert(app_.new_files_with_categories.end(),
                                              other_results.begin(),
                                              other_results.end());
        app_.new_files_with_categories.insert(app_.new_files_with_categories.end(),
                                              image_results.begin(),
                                              image_results.end());
        app_.new_files_with_categories.insert(app_.new_files_with_categories.end(),
                                              document_results.begin(),
                                              document_results.end());

        app_.core_logger->info("Categorization produced {} new record(s).",
                               app_.new_files_with_categories.size());

        app_.already_categorized_files.insert(app_.already_categorized_files.end(),
                                              app_.new_files_with_categories.begin(),
                                              app_.new_files_with_categories.end());

        persist_analysis_results(app_.new_files_with_categories);

        std::vector<CategorizedFile> review_entries = app_.already_categorized_files;
        if ((rename_images_only || rename_documents_only) && !pending_renames.empty()) {
            review_entries.insert(review_entries.end(), pending_renames.begin(), pending_renames.end());
        }

        const auto actual_files = app_.results_coordinator.list_directory(app_.get_folder_path(), scan_options);
        app_.new_files_to_sort = app_.results_coordinator.compute_files_to_sort(
            app_.get_folder_path(),
            scan_options,
            actual_files,
            review_entries,
            app_.settings.get_include_subdirectories());
        app_.core_logger->debug("{} file(s) queued for sorting after analysis.",
                                app_.new_files_to_sort.size());

        const bool cancelled = stop_requested;
        app_.run_on_ui([this, cancelled]() {
            if (cancelled && app_.new_files_to_sort.empty()) {
                app_.handle_analysis_cancelled();
            } else {
                app_.handle_analysis_finished();
            }
        });
    } catch (const std::exception& ex) {
        app_.core_logger->error("Exception during analysis: {}", ex.what());
        const bool cancelled =
            app_.stop_analysis.load() ||
            (app_.text_cpu_fallback_choice_.has_value() && !app_.text_cpu_fallback_choice_.value());
        if (cancelled) {
            app_.run_on_ui([this]() { app_.handle_analysis_cancelled(); });
        } else {
            app_.run_on_ui([this, message = std::string("Analysis error: ") + ex.what()]() {
                app_.handle_analysis_failure(message);
            });
        }
    }
}
