#pragma once

#ifdef AI_FILE_SORTER_TEST_BUILD

#include <optional>
#include <string>

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
    static void select_visual_backend(LLMSelectionDialog& dialog, const std::string& backend_id);
    static void set_network_available_override(LLMSelectionDialog& dialog, std::optional<bool> value);
};

#endif // AI_FILE_SORTER_TEST_BUILD
