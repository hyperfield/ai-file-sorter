#ifndef SUITABILITY_BENCHMARK_DIALOG_HPP
#define SUITABILITY_BENCHMARK_DIALOG_HPP

#include "AnalysisRuntimeLock.hpp"
#include "Types.hpp"
#include "SuitabilityBenchmarkResults.hpp"

#include <QCoreApplication>
#include <QDialog>
#include <QStringList>

#include <atomic>
#include <memory>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
#include <functional>

class QLabel;
class QTextEdit;
class QProgressBar;
class QPushButton;
class QCheckBox;
class Settings;
class QCloseEvent;
class QEvent;
class QComboBox;
class QTableWidget;

#if defined(AI_FILE_SORTER_TEST_BUILD)
namespace SuitabilityBenchmarkDialogTestAccess { struct DialogAccess; }
#endif

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
     * @param runtime_directory Shared analysis lock directory, or empty for the settings default.
     */
    SuitabilityBenchmarkDialog(Settings& settings,
                               QWidget* parent = nullptr,
                               std::filesystem::path runtime_directory = {});
    /** @brief Check for any usable local text or visual model files, including custom models. */
    static bool models_available(const Settings& settings);
    void reject() override;
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
#if defined(AI_FILE_SORTER_TEST_BUILD)
    friend struct SuitabilityBenchmarkDialogTestAccess::DialogAccess;
#endif
    struct BenchmarkInputs {
        std::string storage_dir;
        std::vector<CustomLLM> custom_llms;
        SortingMode sorting_mode{SortingMode::GeneratedCategories};
        std::string backend_mode{"auto"};
        bool retry{false};
        SuitabilityBenchmark::Results requests;
    };
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
    void start_attempts(SuitabilityBenchmark::Results requests, bool retry);
    void retry_selected(bool change_backend);
    void retry_failed();
    void refresh_results_table();
    void update_retry_controls();
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
     * @param completed True only after every benchmark stage has finished normally.
     */
    void finish_benchmark(bool completed);
    /**
     * @brief Load the most recent benchmark results from settings.
     */
    void load_previous_results();
    /**
     * @brief Render saved benchmark results into the output view.
     */
    void render_previous_results();

    Settings& settings_;
    AnalysisRuntimeLock runtime_lock_;
    std::optional<AnalysisRuntimeLock::Lease> runtime_lease_;
    BenchmarkInputs inputs_;
    SuitabilityBenchmark::Results results_;
    SuitabilityBenchmark::Results pending_results_;
    bool pending_results_ready_{false};
    QComboBox* backend_combo_{nullptr};
    QTableWidget* results_table_{nullptr};
    QPushButton* retry_selected_button_{nullptr};
    QPushButton* retry_failed_button_{nullptr};
    QPushButton* try_backend_button_{nullptr};
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
#if defined(AI_FILE_SORTER_TEST_BUILD)
    std::function<SuitabilityBenchmark::Result(const SuitabilityBenchmark::Result&)> runner_override_;
    std::vector<std::string> available_backends_override_;
#endif
};

#if defined(AI_FILE_SORTER_TEST_BUILD)
/**
 * @brief Test hooks for suitability benchmark backend reporting decisions.
 */
namespace SuitabilityBenchmarkDialogTestAccess {
struct MeasuredModel {
    LLMChoice choice;
    std::string label;
    bool categorization_success;
    bool document_success;
    double categorization_seconds;
    double document_seconds;
};
QStringList result_lines_for_testing(const std::vector<MeasuredModel>& models, SortingMode mode);
std::vector<std::string> local_model_paths_for_testing(const std::vector<CustomLLM>& custom_llms);
std::pair<double, double> thresholds_for_testing(LLMChoice choice, bool document);
struct DialogAccess {
    static void start(SuitabilityBenchmarkDialog& dialog);
    static void set_running(SuitabilityBenchmarkDialog& dialog, bool running);
    static void finish(SuitabilityBenchmarkDialog& dialog, bool completed);
    static void seed_results(SuitabilityBenchmarkDialog& dialog, SuitabilityBenchmark::Results results);
    static SuitabilityBenchmark::Results results(const SuitabilityBenchmarkDialog& dialog);
    static void set_runner(SuitabilityBenchmarkDialog& dialog,
        std::function<SuitabilityBenchmark::Result(const SuitabilityBenchmark::Result&)> runner,
        std::vector<std::string> available_backends);
    static QStringList summary(const SuitabilityBenchmarkDialog& dialog);
    static bool running(const SuitabilityBenchmarkDialog& dialog);
};
bool backend_alias_matches(std::string_view actual, std::string_view requested);
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
