/**
 * @file MainAppTestAccess.hpp
 * @brief Test-only accessors and helpers for MainApp.
 */
#pragma once

#ifdef AI_FILE_SORTER_TEST_BUILD

#include "Types.hpp"

#include <QString>
#include <QCheckBox>
#include <QToolButton>
#include <functional>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

class MainApp;
class Settings;
class QAction;
class QMenu;
class QLineEdit;
class QPushButton;
struct StorageProviderDetection;

/**
 * @brief Provides test access to MainApp internals and helpers.
 */
class MainAppTestAccess {
public:
    /**
     * @brief Simulated responses for the support prompt flow.
     */
    enum class SimulatedSupportResult {
        /// User entered a valid donation code and hid the prompt.
        Support,
        /// User is unsure.
        NotSure
    };

    /**
     * @brief Read the analyze button label text.
     * @param app MainApp instance.
     * @return Current analyze button text.
     */
    static QString analyze_button_text(const MainApp& app);
    /**
     * @brief Read the folder/path label text.
     * @param app MainApp instance.
     * @return Current path label text.
     */
    static QString path_label_text(const MainApp& app);
    /**
     * @brief Access the folder/path line edit.
     * @param app MainApp instance.
     * @return Pointer to the line edit, or nullptr if unavailable.
     */
    static QLineEdit* path_entry(MainApp& app);
    /**
     * @brief Access the folder browse button.
     * @param app MainApp instance.
     * @return Pointer to the button, or nullptr if unavailable.
     */
    static QPushButton* browse_button(MainApp& app);
    /**
     * @brief Access the main analyze button.
     * @param app MainApp instance.
     * @return Pointer to the button, or nullptr if unavailable.
     */
    static QPushButton* analyze_button(MainApp& app);
    /**
     * @brief Access the Settings -> Clear cache action.
     * @param app MainApp instance.
     * @return Pointer to the action, or nullptr if unavailable.
     */
    static QAction* clear_cache_action(MainApp& app);
    /**
     * @brief Access the Settings -> Reset learned behavior action.
     * @param app MainApp instance.
     * @return Pointer to the action, or nullptr if unavailable.
     */
    static QAction* reset_learning_action(MainApp& app);
    /**
     * @brief Access the Settings -> Category language menu.
     * @param app MainApp instance.
     * @return Pointer to the menu, or nullptr if unavailable.
     */
    static QMenu* category_language_menu(MainApp& app);
    /**
     * @brief Access the Settings menu.
     * @param app MainApp instance.
     * @return Pointer to the Settings menu, or nullptr if unavailable.
     */
    static QMenu* settings_menu(MainApp& app);
    /**
     * @brief Access the development-only Plugins menu.
     * @param app MainApp instance.
     * @return Pointer to the Plugins menu, or nullptr if hidden.
     */
    static QMenu* plugins_menu(MainApp& app);
    /**
     * @brief Access the development-only storage plugin management action.
     * @param app MainApp instance.
     * @return Pointer to the action, or nullptr if hidden.
     */
    static QAction* manage_storage_plugins_action(MainApp& app);
    /**
     * @brief Access the test-mode Tests menu.
     * @param app MainApp instance.
     * @return Pointer to the Tests menu, or nullptr if hidden.
     */
    static QMenu* test_menu(MainApp& app);
    /**
     * @brief Access the test-mode large whitelist LLM test action.
     * @param app MainApp instance.
     * @return Pointer to the action, or nullptr if hidden.
     */
    static QAction* run_large_whitelist_llm_test_action(MainApp& app);
    /**
     * @brief Access the \"Categorize files\" checkbox.
     * @param app MainApp instance.
     * @return Pointer to the checkbox, or nullptr if unavailable.
     */
    static QCheckBox* categorize_files_checkbox(MainApp& app);
    /**
     * @brief Resolve the effective scan options currently selected in the UI.
     * @param app MainApp instance.
     * @return Effective scan options used for enumeration.
     */
    static FileScanOptions effective_scan_options(const MainApp& app);
    /**
     * @brief Access the \"Analyze picture files\" checkbox.
     * @param app MainApp instance.
     * @return Pointer to the checkbox, or nullptr if unavailable.
     */
    static QCheckBox* analyze_images_checkbox(MainApp& app);
    /**
     * @brief Access the \"Process picture files only\" checkbox.
     * @param app MainApp instance.
     * @return Pointer to the checkbox, or nullptr if unavailable.
     */
    static QCheckBox* process_images_only_checkbox(MainApp& app);
    /**
     * @brief Access the \"Add image creation date to category name\" checkbox.
     * @param app MainApp instance.
     * @return Pointer to the checkbox, or nullptr if unavailable.
     */
    static QCheckBox* add_image_date_to_category_checkbox(MainApp& app);
    /**
     * @brief Access the \"Add photo date and place to filename\" checkbox.
     * @param app MainApp instance.
     * @return Pointer to the checkbox, or nullptr if unavailable.
     */
    static QCheckBox* add_image_date_place_to_filename_checkbox(MainApp& app);
    /**
     * @brief Access the \"Add audio/video metadata to file name\" checkbox.
     * @param app MainApp instance.
     * @return Pointer to the checkbox, or nullptr if unavailable.
     */
    static QCheckBox* add_audio_video_metadata_to_filename_checkbox(MainApp& app);
    /**
     * @brief Access the \"Offer to rename picture files\" checkbox.
     * @param app MainApp instance.
     * @return Pointer to the checkbox, or nullptr if unavailable.
     */
    static QCheckBox* offer_rename_images_checkbox(MainApp& app);
    /**
     * @brief Access the \"Do not categorize picture files\" checkbox.
     * @param app MainApp instance.
     * @return Pointer to the checkbox, or nullptr if unavailable.
     */
    static QCheckBox* rename_images_only_checkbox(MainApp& app);
    /**
     * @brief Access the \"Analyze document files\" checkbox.
     * @param app MainApp instance.
     * @return Pointer to the checkbox, or nullptr if unavailable.
     */
    static QCheckBox* analyze_documents_checkbox(MainApp& app);
    /**
     * @brief Access the \"Process document files only\" checkbox.
     * @param app MainApp instance.
     * @return Pointer to the checkbox, or nullptr if unavailable.
     */
    static QCheckBox* process_documents_only_checkbox(MainApp& app);
    /**
     * @brief Access the \"Do not categorize document files\" checkbox.
     * @param app MainApp instance.
     * @return Pointer to the checkbox, or nullptr if unavailable.
     */
    static QCheckBox* rename_documents_only_checkbox(MainApp& app);
    /**
     * @brief Access the picture-analysis disclosure toggle.
     * @param app MainApp instance.
     * @return Pointer to the toggle button, or nullptr if unavailable.
     */
    static QToolButton* image_options_toggle_button(MainApp& app);
    /**
     * @brief Access the document-analysis disclosure toggle.
     * @param app MainApp instance.
     * @return Pointer to the toggle button, or nullptr if unavailable.
     */
    static QToolButton* document_options_toggle_button(MainApp& app);
    /**
     * @brief Split file entries into image/document/other buckets for analysis.
     * @param files Input entries to split.
     * @param analyze_images Whether to analyze images by content.
     * @param analyze_documents Whether to analyze documents by content.
     * @param process_images_only Whether only images should be processed.
     * @param process_documents_only Whether only documents should be processed.
     * @param rename_images_only Whether images should be rename-only.
     * @param rename_documents_only Whether documents should be rename-only.
     * @param categorize_files Whether non-analyzed files are eligible for categorization.
     * @param use_full_path_keys Whether to key renamed files by full path.
     * @param renamed_files Set of already-renamed file keys.
     * @param image_entries Output vector of image entries.
     * @param document_entries Output vector of document entries.
     * @param other_entries Output vector of other entries.
     */
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
    /**
     * @brief Override the probe used to detect visual LLM availability.
     * @param app MainApp instance.
     * @param probe Callback returning availability state.
     */
    static void set_visual_llm_available_probe(MainApp& app, std::function<bool()> probe);
    /**
     * @brief Override the visual LLM selection dialog runner.
     * @param app MainApp instance.
     * @param runner Callback invoked instead of showing the dialog.
     */
    static void set_llm_selection_runner(MainApp& app, std::function<void()> runner);
    /**
     * @brief Override the image analysis prompt flow.
     * @param app MainApp instance.
     * @param prompt Callback that returns whether to proceed.
     */
    static void set_image_analysis_prompt_override(MainApp& app, std::function<bool()> prompt);
    /**
     * @brief Override the visual CPU fallback prompt flow.
     * @param app MainApp instance.
     * @param prompt Callback returning whether CPU retry is accepted.
     */
    static void set_visual_cpu_fallback_prompt_override(MainApp& app, std::function<bool()> prompt);
    /**
     * @brief Override the continue-without-visual-analysis prompt flow.
     * @param app MainApp instance.
     * @param prompt Callback returning whether filename-only continuation is accepted.
     */
    static void set_continue_without_visual_analysis_prompt_override(
        MainApp& app,
        std::function<bool()> prompt);
    /**
     * @brief Invoke the visual CPU fallback prompt path.
     * @param app MainApp instance.
     * @param reason Failure reason to report.
     * @return True when CPU retry is accepted.
     */
    static bool prompt_visual_cpu_fallback(MainApp& app, const std::string& reason);
    /**
     * @brief Invoke the continue-without-visual-analysis prompt path.
     * @param app MainApp instance.
     * @param reason Failure reason to report.
     * @return True when filename-only continuation is accepted.
     */
    static bool prompt_continue_without_visual_analysis(MainApp& app, const std::string& reason);
    /**
     * @brief Return whether analysis cancellation has been requested.
     * @param app MainApp instance.
     * @return True when the stop-analysis flag is set.
     */
    static bool stop_analysis_requested(const MainApp& app);
    /**
     * @brief Return whether a progress message would be shown in the progress dialog.
     * @param app MainApp instance.
     * @param message Progress text to evaluate.
     * @return True when the dialog should show the message.
     */
    static bool should_show_progress_message_in_dialog(const MainApp& app,
                                                       const std::string& message);
    /**
     * @brief Returns whether a visual-analysis failure should offer CPU retry.
     * @param reason Exception text produced by the failed visual analysis step.
     * @return True when the failure looks like GPU memory pressure.
     */
    static bool should_offer_visual_cpu_fallback(const std::string& reason);
    /**
     * @brief Resolve the prompt filename used for document categorization.
     * @param original_name Original file name.
     * @param suggested_name Suggested file name, when available.
     * @return Suggested name when present; otherwise the original name.
     */
    static std::string resolve_document_prompt_name(const std::string& original_name,
                                                    const std::string& suggested_name);
    /**
     * @brief Build the image prompt path shown in categorization progress.
     * @param full_path Original full path to the image.
     * @param prompt_name File name to use in the categorization prompt.
     * @param description Optional visual description appended for the LLM prompt.
     * @return Prompt path string used for categorization.
     */
    static std::string build_image_prompt_path(const std::string& full_path,
                                               const std::string& prompt_name,
                                               const std::string& description);
    /**
     * @brief Build the document prompt path shown in categorization progress.
     * @param full_path Original full path to the document.
     * @param prompt_name File name to use in the categorization prompt.
     * @param summary Optional summary appended for the LLM prompt.
     * @return Prompt path string used for categorization.
     */
    static std::string build_document_prompt_path(const std::string& full_path,
                                                  const std::string& prompt_name,
                                                  const std::string& summary);
    /**
     * @brief Trigger a UI retranslate on the MainApp instance.
     * @param app MainApp instance.
     */
    static void trigger_retranslate(MainApp& app);
    /**
     * @brief Re-evaluate category-language visibility for the current LLM choice.
     * @param app MainApp instance.
     */
    static void refresh_category_language_menu(MainApp& app);
    /**
     * @brief Record a count of categorized files for metrics.
     * @param app MainApp instance.
     * @param count Number of files to add.
     */
    static void add_categorized_files(MainApp& app, int count);
    /**
     * @brief Force the analysis-in-progress UI state for action enablement tests.
     * @param app MainApp instance.
     * @param analyzing True to simulate an active analysis.
     */
    static void set_analysis_in_progress(MainApp& app, bool analyzing);
    /**
     * @brief Resolve the storage-support state for a detected provider.
     * @param app MainApp instance.
     * @param detection Provider detection result to classify.
     * @return State name used by tests.
     */
    static std::string resolve_storage_support_state_name(
        MainApp& app,
        const StorageProviderDetection& detection);
    /**
     * @brief Resolve the matched plugin id for a detected provider, if any.
     * @param app MainApp instance.
     * @param detection Provider detection result to classify.
     * @return Matching plugin id when available.
     */
    static std::optional<std::string> resolve_storage_support_plugin_id(
        MainApp& app,
        const StorageProviderDetection& detection);
    /**
     * @brief Simulate the support prompt logic for tests.
     * @param settings Settings instance to update.
     * @param prompt_state Prompt state flag to mutate.
     * @param count Number of files categorized in this increment.
     * @param callback Callback to supply a simulated response.
     */
    static void simulate_support_prompt(Settings& settings,
                                        bool& prompt_state,
                                        int count,
                                        std::function<SimulatedSupportResult(int)> callback);
};

#endif // AI_FILE_SORTER_TEST_BUILD
