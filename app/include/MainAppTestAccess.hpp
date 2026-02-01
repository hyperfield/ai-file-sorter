/**
 * @file MainAppTestAccess.hpp
 * @brief Test-only accessors and helpers for MainApp.
 */
#pragma once

#ifdef AI_FILE_SORTER_TEST_BUILD

#include "Types.hpp"

#include <QString>
#include <functional>
#include <QCheckBox>
#include <unordered_set>
#include <vector>

class MainApp;
class Settings;

/**
 * @brief Provides test access to MainApp internals and helpers.
 */
class MainAppTestAccess {
public:
    /**
     * @brief Simulated responses for the support prompt flow.
     */
    enum class SimulatedSupportResult {
        /// User agreed to support.
        Support,
        /// User is unsure.
        NotSure,
        /// User cannot donate.
        CannotDonate
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
     * @brief Access the \"Categorize files\" checkbox.
     * @param app MainApp instance.
     * @return Pointer to the checkbox, or nullptr if unavailable.
     */
    static QCheckBox* categorize_files_checkbox(MainApp& app);
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
     * @brief Trigger a UI retranslate on the MainApp instance.
     * @param app MainApp instance.
     */
    static void trigger_retranslate(MainApp& app);
    /**
     * @brief Record a count of categorized files for metrics.
     * @param app MainApp instance.
     * @param count Number of files to add.
     */
    static void add_categorized_files(MainApp& app, int count);
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
