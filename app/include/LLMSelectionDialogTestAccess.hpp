#pragma once

#ifdef AI_FILE_SORTER_TEST_BUILD

#include <optional>
#include <string>
#include <vector>

#include <QComboBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
class LLMDownloader;
class LLMSelectionDialog;

class LLMSelectionDialogTestAccess {
public:
    struct VisualEntryRefs {
        QLabel* status_label{nullptr};
        QPushButton* download_button{nullptr};
        QProgressBar* progress_bar{nullptr};
        LLMDownloader* downloader{nullptr};
    };

    static VisualEntryRefs llava_model_entry(LLMSelectionDialog& dialog);
    static VisualEntryRefs llava_mmproj_entry(LLMSelectionDialog& dialog);
    static void refresh_visual_downloads(LLMSelectionDialog& dialog);
    static void update_llava_model_entry(LLMSelectionDialog& dialog);
    static void start_llava_model_download(LLMSelectionDialog& dialog);
    static VisualEntryRefs visual_entry_for_env_var(LLMSelectionDialog& dialog, const std::string& env_var);
    static std::string selected_visual_model_id(const LLMSelectionDialog& dialog);
    static std::string selected_visual_model_label(const LLMSelectionDialog& dialog);
    /**
     * @brief Returns the visual model selector widget.
     * @param dialog Selection dialog instance under test.
     * @return Visual model combo box, or nullptr when unavailable.
     */
    static QComboBox* visual_model_combo(LLMSelectionDialog& dialog);
    /**
     * @brief Returns the active built-in local model downloader.
     * @param dialog Selection dialog instance under test.
     * @return Downloader used by the local LLM download panel, or nullptr.
     */
    static LLMDownloader* local_downloader(LLMSelectionDialog& dialog);
    /**
     * @brief Returns the visible built-in local model labels in top-to-bottom dialog order.
     * @param dialog Selection dialog instance under test.
     * @return Ordered list of built-in local model labels.
     */
    static std::vector<std::string> local_builtin_labels(const LLMSelectionDialog& dialog);
    static void select_visual_backend(LLMSelectionDialog& dialog, const std::string& backend_id);
    static void set_network_available_override(LLMSelectionDialog& dialog, std::optional<bool> value);
};

#endif // AI_FILE_SORTER_TEST_BUILD
