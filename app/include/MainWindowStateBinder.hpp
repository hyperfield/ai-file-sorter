#pragma once

#include "Types.hpp"

class MainApp;
class QCheckBox;

/**
 * @brief Keeps MainApp settings, checkbox wiring, and dependent UI state in sync.
 */
class MainWindowStateBinder {
public:
    explicit MainWindowStateBinder(MainApp& app);

    void connect_checkbox_signals();
    void connect_whitelist_signals();

    void sync_settings_to_ui();
    void restore_tree_settings();
    void restore_sort_folder_state();
    void restore_file_scan_options();
    void restore_file_explorer_visibility();
    void restore_development_preferences();
    void sync_ui_to_settings();

    void ensure_one_checkbox_active(QCheckBox* changed_checkbox);
    void update_file_scan_option(FileScanOptions option, bool enabled);
    FileScanOptions effective_scan_options() const;
    bool visual_llm_files_available() const;
    void update_image_analysis_controls();
    void update_image_only_controls();
    void update_document_analysis_controls();
    void run_llm_selection_dialog_for_visual();
    void handle_image_analysis_toggle(bool checked);

private:
    MainApp& app_;
};
