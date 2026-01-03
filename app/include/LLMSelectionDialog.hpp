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
        QWidget* container{nullptr};
        QLabel* title_label{nullptr};
        QLabel* remote_url_label{nullptr};
        QLabel* local_path_label{nullptr};
        QLabel* file_size_label{nullptr};
        QLabel* status_label{nullptr};
        QProgressBar* progress_bar{nullptr};
        QPushButton* download_button{nullptr};
        std::unique_ptr<LLMDownloader> downloader;
        std::atomic<bool> is_downloading{false};
    };

    void setup_ui();
    void connect_signals();
    void update_ui_for_choice();
    void update_radio_selection();
    void update_custom_choice_ui();
    void update_openai_fields_state();
    void update_gemini_fields_state();
    bool openai_inputs_valid() const;
    bool gemini_inputs_valid() const;
    void update_local_choice_ui();
    void update_download_info();
    void start_download();
    void refresh_downloader();
    void set_status_message(const QString& message);
    std::string current_download_env_var() const;
    void refresh_custom_lists();
    void handle_add_custom();
    void handle_edit_custom();
    void handle_delete_custom();
    void update_custom_buttons();
    void select_custom_by_id(const std::string& id);
    void setup_visual_llm_download_entry(VisualLlmDownloadEntry& entry,
                                     QWidget* parent,
                                     const QString& title,
                                     const std::string& env_var);
    void refresh_visual_llm_download_entry(VisualLlmDownloadEntry& entry);
    void update_visual_llm_download_entry(VisualLlmDownloadEntry& entry);
    void update_visual_llm_downloads();
    void start_visual_llm_download(VisualLlmDownloadEntry& entry);
    void set_visual_status_message(VisualLlmDownloadEntry& entry, const QString& message);

    Settings& settings;
    LLMChoice selected_choice{LLMChoice::Unset};
    std::string selected_custom_id;
    std::string openai_api_key;
    std::string openai_model;
    std::string gemini_api_key;
    std::string gemini_model;

    QRadioButton* openai_radio{nullptr};
    QRadioButton* gemini_radio{nullptr};
    QRadioButton* local3_radio{nullptr};
    QRadioButton* local7_radio{nullptr};
    QRadioButton* custom_radio{nullptr};
    QComboBox* custom_combo{nullptr};
    QPushButton* add_custom_button{nullptr};
    QPushButton* edit_custom_button{nullptr};
    QPushButton* delete_custom_button{nullptr};
    QLabel* remote_url_label{nullptr};
    QLabel* local_path_label{nullptr};
    QLabel* file_size_label{nullptr};
    QLabel* status_label{nullptr};
    QProgressBar* progress_bar{nullptr};
    QPushButton* download_button{nullptr};
    QPushButton* ok_button{nullptr};
    QDialogButtonBox* button_box{nullptr};
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
