#ifndef SUITABILITY_BENCHMARK_DIALOG_HPP
#define SUITABILITY_BENCHMARK_DIALOG_HPP

#include <QCoreApplication>
#include <QDialog>
#include <QStringList>

#include <atomic>
#include <memory>
#include <string_view>
#include <thread>

class QLabel;
class QTextEdit;
class QProgressBar;
class QPushButton;
class QCheckBox;
class Settings;
class QCloseEvent;
class QEvent;

/**
 * @brief Dialog that runs a suitability benchmark for categorization and analysis features.
 */
class SuitabilityBenchmarkDialog : public QDialog
{
    Q_DECLARE_TR_FUNCTIONS(SuitabilityBenchmarkDialog)
public:
    /**
     * @brief Create a suitability benchmark dialog.
     * @param settings Settings store used for persistence.
     * @param parent Parent widget.
     */
    SuitabilityBenchmarkDialog(Settings& settings,
                               QWidget* parent = nullptr);
    /**
     * @brief Destructor that joins any running benchmark thread.
     */
    ~SuitabilityBenchmarkDialog() override;

protected:
    /**
     * @brief Handle language changes for translated UI strings.
     * @param event Qt event payload.
     */
    void changeEvent(QEvent* event) override;
    /**
     * @brief Prevent closing while the benchmark is running.
     * @param event Qt close event.
     */
    void closeEvent(QCloseEvent* event) override;

private:
    /**
     * @brief Build and connect the dialog UI elements.
     */
    void setup_ui();
    /**
     * @brief Apply platform-specific presentation tweaks after widgets exist.
     */
    void apply_platform_styling();
    /**
     * @brief Update translated strings for the dialog.
     */
    void retranslate_ui();
    /**
     * @brief Start the benchmark worker thread and clear prior output.
     */
    void start_benchmark();
    /**
     * @brief Request the benchmark to stop after the current step finishes.
     */
    void request_stop();
    /**
     * @brief Run benchmark steps off the UI thread.
     */
    void run_benchmark_worker();
    /**
     * @brief Append a line to the output view.
     * @param text Line to display.
     * @param is_html True when the text already contains HTML markup.
     */
    void append_line(const QString& text, bool is_html);
    /**
     * @brief Toggle UI state for a running benchmark.
     * @param running True when the benchmark is running.
     */
    void set_running_state(bool running);
    /**
     * @brief Finish the benchmark and persist results to settings.
     */
    void finish_benchmark();
    /**
     * @brief Load the most recent benchmark results from settings.
     */
    void load_previous_results();
    /**
     * @brief Render saved benchmark results into the output view.
     */
    void render_previous_results();

    Settings& settings_;
    QLabel* intro_label_{nullptr};
    QTextEdit* output_view_{nullptr};
    QProgressBar* progress_bar_{nullptr};
    QCheckBox* suppress_checkbox_{nullptr};
    QPushButton* run_button_{nullptr};
    QPushButton* stop_button_{nullptr};
    QPushButton* close_button_{nullptr};
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::thread worker_;
    bool recording_{false};
    bool showing_previous_results_{false};
    QString last_run_stamp_;
    QString last_report_;
    QStringList current_report_;
};

#if defined(AI_FILE_SORTER_TEST_BUILD)
/**
 * @brief Test hooks for suitability benchmark backend reporting decisions.
 */
namespace SuitabilityBenchmarkDialogTestAccess {
/**
 * @brief Build the benchmark backend note for a requested backend and probed accelerators.
 * @param requested_backend Requested backend key such as `cuda`, `vulkan`, `cpu`, or empty auto.
 * @param cuda_available True when CUDA is usable.
 * @param vulkan_available True when Vulkan is usable.
 * @param metal_available True when Metal is usable.
 * @param gpu_fallback True when the measured run fell back from GPU to CPU.
 * @return User-facing backend note.
 */
QString backend_note_for_testing(std::string_view requested_backend,
                                 bool cuda_available,
                                 bool vulkan_available,
                                 bool metal_available,
                                 bool gpu_fallback);
/**
 * @brief Build the image-analysis backend note for a requested backend and probed accelerators.
 * @param requested_backend Requested backend key such as `cuda`, `vulkan`, `cpu`, or empty auto.
 * @param gpu_enabled True when image analysis was allowed to use GPU acceleration.
 * @param cuda_available True when CUDA is usable.
 * @param vulkan_available True when Vulkan is usable.
 * @param metal_available True when Metal is usable.
 * @return User-facing image backend note.
 */
QString image_backend_note_for_testing(std::string_view requested_backend,
                                       bool gpu_enabled,
                                       bool cuda_available,
                                       bool vulkan_available,
                                       bool metal_available);
} // namespace SuitabilityBenchmarkDialogTestAccess
#endif

#endif // SUITABILITY_BENCHMARK_DIALOG_HPP
