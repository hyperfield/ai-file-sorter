#ifndef LLMSELECTIONDIALOG_HPP
#define LLMSELECTIONDIALOG_HPP

#include "LLMDownloader.hpp"
#include "Types.hpp"

#include <QDialog>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

class QLabel;
class QProgressBar;
class QPushButton;
class QRadioButton;
class QDialogButtonBox;
class QWidget;
class QString;
class QComboBox;
class QListWidget;
class QLineEdit;
class QCheckBox;
class QToolButton;
class QScrollArea;
class QShowEvent;

class Settings;

#ifdef AI_FILE_SORTER_TEST_BUILD
class LLMSelectionDialogTestAccess;
#endif

class LLMSelectionDialog : public QDialog
{
public:
    explicit LLMSelectionDialog(Settings& settings, QWidget* parent = nullptr);
    ~LLMSelectionDialog() override;

    LLMChoice get_selected_llm_choice() const;
    std::string get_selected_custom_llm_id() const;
    /**
     * @brief Return the active custom API endpoint id.
     */
    std::string get_selected_custom_api_id() const;
    std::string get_openai_api_key() const;
    std::string get_openai_model() const;
    std::string get_gemini_api_key() const;
    std::string get_gemini_model() const;

private:
#ifdef AI_FILE_SORTER_TEST_BUILD
    friend class LLMSelectionDialogTestAccess;
#endif

    struct VisualLlmDownloadEntry {
        std::string env_var;
        std::string display_name;
        QWidget* container{nullptr};
        QLabel* title_label{nullptr};
        QLabel* remote_url_label{nullptr};
        QLabel* local_path_label{nullptr};
        QLabel* file_size_label{nullptr};
        QLabel* status_label{nullptr};
        QProgressBar* progress_bar{nullptr};
        QPushButton* download_button{nullptr};
        QPushButton* delete_button{nullptr};
        std::unique_ptr<LLMDownloader> downloader;
        std::atomic<bool> is_downloading{false};
    };

    void setup_ui();
    void connect_signals();
    void showEvent(QShowEvent* event) override;
    void update_ui_for_choice();
    void update_legacy_local_3b_visibility();
    void update_radio_selection();
    void update_custom_choice_ui();
    /**
     * @brief Update UI state for the custom API selection.
     */
    void update_custom_api_choice_ui();
    void update_openai_fields_state();
    void update_gemini_fields_state();
    bool openai_inputs_valid() const;
    bool gemini_inputs_valid() const;
    void update_local_choice_ui();
    void update_download_info();
    void start_download();
    /**
     * @brief Delete the downloaded local LLM file after confirmation.
     */
    void handle_delete_download();
    void refresh_downloader();
    void set_status_message(const QString& message);
    std::string current_download_env_var() const;
    void refresh_custom_lists();
    /**
     * @brief Refresh the custom API dropdown list.
     */
    void refresh_custom_api_lists();
    void handle_add_custom();
    void handle_edit_custom();
    void handle_delete_custom();
    void update_custom_buttons();
    void select_custom_by_id(const std::string& id);
    /**
     * @brief Open the dialog to add a custom API entry.
     */
    void handle_add_custom_api();
    /**
     * @brief Open the dialog to edit the selected custom API entry.
     */
    void handle_edit_custom_api();
    /**
     * @brief Remove the selected custom API entry.
     */
    void handle_delete_custom_api();
    /**
     * @brief Update enabled states for custom API controls.
     */
    void update_custom_api_buttons();
    /**
     * @brief Select a custom API entry by id.
     */
    void select_custom_api_by_id(const std::string& id);
    /**
     * @brief Recalculate the dialog size based on visible content.
     */
    void adjust_dialog_size();
    void setup_visual_llm_download_entry(VisualLlmDownloadEntry& entry,
                                     QWidget* parent,
                                     const QString& title,
                                     const std::string& env_var);
    void refresh_visual_llm_download_entry(VisualLlmDownloadEntry& entry);
    void update_visual_llm_download_entry(VisualLlmDownloadEntry& entry);
    void update_visual_llm_downloads();
    void start_visual_llm_download(VisualLlmDownloadEntry& entry);
    /**
     * @brief Delete a downloaded visual LLM bundle after confirmation.
     * @param entry Visual LLM entry to delete.
     */
    void handle_delete_visual_download(VisualLlmDownloadEntry& entry);
    void set_visual_status_message(VisualLlmDownloadEntry& entry, const QString& message);
    bool legacy_local_3b_available() const;

    Settings& settings;
    LLMChoice selected_choice{LLMChoice::Unset};
    std::string selected_custom_id;
    std::string selected_custom_api_id;
    std::string openai_api_key;
    std::string openai_model;
    std::string gemini_api_key;
    std::string gemini_model;

    QRadioButton* openai_radio{nullptr};
    QRadioButton* gemini_radio{nullptr};
    QRadioButton* custom_api_radio{nullptr};
    QRadioButton* local3_radio{nullptr};
    QRadioButton* local3_legacy_radio{nullptr};
    QRadioButton* local7_radio{nullptr};
    QRadioButton* custom_radio{nullptr};
    QToolButton* download_toggle_button{nullptr};
    QScrollArea* scroll_area_{nullptr};
    QComboBox* custom_combo{nullptr};
    QPushButton* add_custom_button{nullptr};
    QPushButton* edit_custom_button{nullptr};
    QPushButton* delete_custom_button{nullptr};
    QComboBox* custom_api_combo{nullptr};
    QPushButton* add_custom_api_button{nullptr};
    QPushButton* edit_custom_api_button{nullptr};
    QPushButton* delete_custom_api_button{nullptr};
    QLabel* remote_url_label{nullptr};
    QLabel* local_path_label{nullptr};
    QLabel* file_size_label{nullptr};
    QLabel* status_label{nullptr};
    QLabel* local3_legacy_desc{nullptr};
    QProgressBar* progress_bar{nullptr};
    QPushButton* download_button{nullptr};
    QPushButton* delete_download_button{nullptr};
    QPushButton* ok_button{nullptr};
    QDialogButtonBox* button_box{nullptr};
    QWidget* downloads_container{nullptr};
    QWidget* download_section{nullptr};
    QWidget* visual_llm_download_section{nullptr};
    QWidget* openai_inputs{nullptr};
    QWidget* gemini_inputs{nullptr};
    QLineEdit* openai_api_key_edit{nullptr};
    QLineEdit* openai_model_edit{nullptr};
    QLineEdit* gemini_api_key_edit{nullptr};
    QLineEdit* gemini_model_edit{nullptr};
    QCheckBox* show_openai_api_key_checkbox{nullptr};
    QCheckBox* show_gemini_api_key_checkbox{nullptr};
    QLabel* openai_help_label{nullptr};
    QLabel* openai_link_label{nullptr};
    QLabel* gemini_help_label{nullptr};
    QLabel* gemini_link_label{nullptr};

    std::unique_ptr<LLMDownloader> downloader;
    std::atomic<bool> is_downloading{false};
    std::mutex download_mutex;

    VisualLlmDownloadEntry llava_model_download;
    VisualLlmDownloadEntry llava_mmproj_download;

#if defined(AI_FILE_SORTER_TEST_BUILD)
    bool use_network_available_override_{false};
    bool network_available_override_{true};
#endif
};

#endif // LLMSELECTIONDIALOG_HPP
