#include "MainAppTestAccess.hpp"

#ifdef AI_FILE_SORTER_TEST_BUILD

#include "AnalysisCoordinator.hpp"
#include "AnalysisEntryRouter.hpp"
#include "MainApp.hpp"
#include "SupportCodeManager.hpp"
#include "Utils.hpp"
#include "VisualLlmRuntime.hpp"

#include <QLabel>
#include <QPushButton>
#include <QToolButton>

#include <algorithm>

QString MainAppTestAccess::analyze_button_text(const MainApp& app) {
    return app.analyze_button ? app.analyze_button->text() : QString();
}

QString MainAppTestAccess::path_label_text(const MainApp& app) {
    return app.path_label ? app.path_label->text() : QString();
}

QCheckBox* MainAppTestAccess::categorize_files_checkbox(MainApp& app) {
    return app.categorize_files_checkbox;
}

QCheckBox* MainAppTestAccess::analyze_images_checkbox(MainApp& app) {
    return app.analyze_images_checkbox;
}

QCheckBox* MainAppTestAccess::process_images_only_checkbox(MainApp& app) {
    return app.process_images_only_checkbox;
}

QCheckBox* MainAppTestAccess::add_image_date_to_category_checkbox(MainApp& app) {
    return app.add_image_date_to_category_checkbox;
}

QCheckBox* MainAppTestAccess::add_image_date_place_to_filename_checkbox(MainApp& app) {
    return app.add_image_date_place_to_filename_checkbox;
}

QCheckBox* MainAppTestAccess::add_audio_video_metadata_to_filename_checkbox(MainApp& app) {
    return app.add_audio_video_metadata_to_filename_checkbox;
}

QCheckBox* MainAppTestAccess::offer_rename_images_checkbox(MainApp& app) {
    return app.offer_rename_images_checkbox;
}

QCheckBox* MainAppTestAccess::rename_images_only_checkbox(MainApp& app) {
    return app.rename_images_only_checkbox;
}

QCheckBox* MainAppTestAccess::analyze_documents_checkbox(MainApp& app) {
    return app.analyze_documents_checkbox;
}

QCheckBox* MainAppTestAccess::process_documents_only_checkbox(MainApp& app) {
    return app.process_documents_only_checkbox;
}

QCheckBox* MainAppTestAccess::rename_documents_only_checkbox(MainApp& app) {
    return app.rename_documents_only_checkbox;
}

QToolButton* MainAppTestAccess::image_options_toggle_button(MainApp& app) {
    return app.image_options_toggle_button;
}

QToolButton* MainAppTestAccess::document_options_toggle_button(MainApp& app) {
    return app.document_options_toggle_button;
}

void MainAppTestAccess::split_entries_for_analysis(const std::vector<FileEntry>& files,
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
                                                   std::vector<FileEntry>& other_entries) {
    AnalysisEntryRouter::split_entries_for_analysis(files,
                                                    analyze_images,
                                                    analyze_documents,
                                                    process_images_only,
                                                    process_documents_only,
                                                    rename_images_only,
                                                    rename_documents_only,
                                                    categorize_files,
                                                    use_full_path_keys,
                                                    renamed_files,
                                                    image_entries,
                                                    document_entries,
                                                    other_entries);
}

void MainAppTestAccess::set_visual_llm_available_probe(MainApp& app, std::function<bool()> probe) {
    app.visual_llm_available_probe_ = std::move(probe);
}

void MainAppTestAccess::set_llm_selection_runner(MainApp& app, std::function<void()> runner) {
    app.llm_selection_runner_override_ = std::move(runner);
}

void MainAppTestAccess::set_image_analysis_prompt_override(MainApp& app, std::function<bool()> prompt) {
    app.image_analysis_prompt_override_ = std::move(prompt);
}

bool MainAppTestAccess::should_offer_visual_cpu_fallback(const std::string& reason) {
    return VisualLlmRuntime::should_offer_cpu_fallback(reason);
}

std::string MainAppTestAccess::resolve_document_prompt_name(const std::string& original_name,
                                                            const std::string& suggested_name) {
    return AnalysisCoordinator::resolve_document_prompt_name(original_name, suggested_name);
}

std::string MainAppTestAccess::build_document_prompt_path(const std::string& full_path,
                                                          const std::string& prompt_name,
                                                          const std::string& summary) {
    return AnalysisCoordinator::build_document_prompt_path(full_path, prompt_name, summary);
}

void MainAppTestAccess::trigger_retranslate(MainApp& app) {
    app.retranslate_ui();
}

void MainAppTestAccess::add_categorized_files(MainApp& app, int count) {
    app.record_categorized_metrics(count);
}

std::string MainAppTestAccess::resolve_storage_support_state_name(
    MainApp& app,
    const StorageProviderDetection& detection) {
    const auto resolution = app.resolve_storage_support(detection);
    switch (resolution.state) {
    case MainApp::StorageSupportState::None:
        return "none";
    case MainApp::StorageSupportState::DetectedAndSupportedViaPlugin:
        return "detected_and_supported_via_plugin";
    case MainApp::StorageSupportState::DetectedButPluginNotInstalled:
        return "detected_but_plugin_not_installed";
    case MainApp::StorageSupportState::DetectedButNoPluginExists:
        return "detected_but_no_plugin_exists";
    }
    return "none";
}

std::optional<std::string> MainAppTestAccess::resolve_storage_support_plugin_id(
    MainApp& app,
    const StorageProviderDetection& detection) {
    const auto resolution = app.resolve_storage_support(detection);
    if (!resolution.plugin.has_value()) {
        return std::nullopt;
    }
    return resolution.plugin->id;
}

void MainAppTestAccess::simulate_support_prompt(Settings& settings,
                                                bool& prompt_state,
                                                int count,
                                                std::function<SimulatedSupportResult(int)> callback) {
    if (count <= 0) {
        return;
    }

    settings.add_categorized_files(count);
    settings.save();
    if (prompt_state) {
        return;
    }

    if (SupportCodeManager(Utils::utf8_to_path(settings.get_config_dir()))
            .is_prompt_permanently_disabled()) {
        return;
    }

    const int total = settings.get_total_categorized_files();
    int threshold = settings.get_next_support_prompt_threshold();
    if (threshold <= 0) {
        const int base = std::max(total, 0);
        threshold = ((base / 50) + 1) * 50;
        settings.set_next_support_prompt_threshold(threshold);
        settings.save();
    }

    if (total < threshold || total == 0) {
        return;
    }

    prompt_state = true;
    const SimulatedSupportResult result =
        callback ? callback(total) : SimulatedSupportResult::NotSure;
    prompt_state = false;

    if (result == SimulatedSupportResult::Support) {
        SupportCodeManager(Utils::utf8_to_path(settings.get_config_dir()))
            .force_disable_prompt_for_testing();
        return;
    }

    settings.set_next_support_prompt_threshold(total + 50);
    settings.save();
}

#endif
