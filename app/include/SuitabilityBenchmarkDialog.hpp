#ifndef SUITABILITY_BENCHMARK_DIALOG_HPP
#define SUITABILITY_BENCHMARK_DIALOG_HPP

#include <QDialog>
#include <QStringList>

#include <atomic>
#include <memory>
#include <thread>

class QLabel;
class QTextEdit;
class QProgressBar;
class QPushButton;
class Settings;
class QCloseEvent;
class QEvent;

/**
 * @brief Dialog that runs a suitability benchmark for categorization and analysis features.
 */
class SuitabilityBenchmarkDialog : public QDialog
{
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

#endif // SUITABILITY_BENCHMARK_DIALOG_HPP
