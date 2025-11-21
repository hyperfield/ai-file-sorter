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

class Settings;

class LLMSelectionDialog : public QDialog
{
public:
    explicit LLMSelectionDialog(Settings& settings, QWidget* parent = nullptr);
    ~LLMSelectionDialog() override;

    LLMChoice get_selected_llm_choice() const;
    std::string get_selected_custom_llm_id() const;

private:
    void setup_ui();
    void connect_signals();
    void update_ui_for_choice();
    void update_radio_selection();
    void update_custom_choice_ui();
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

    Settings& settings;
    LLMChoice selected_choice{LLMChoice::Unset};
    std::string selected_custom_id;

    QRadioButton* remote_radio{nullptr};
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
    QDialogButtonBox* button_box{nullptr};
    QWidget* download_section{nullptr};

    std::unique_ptr<LLMDownloader> downloader;
    std::atomic<bool> is_downloading{false};
    std::mutex download_mutex;
};

#endif // LLMSELECTIONDIALOG_HPP
