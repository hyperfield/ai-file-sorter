#include "MainApp.hpp"

#include "Utils.hpp"

#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QTimer>

#include <chrono>
#include <exception>
#include <utility>

namespace {
constexpr int kDepthValidationPollMilliseconds = 75;
}

void MainApp::initialize_destination_depth_validation()
{
    destination_depth_timer_ = new QTimer(this);
    destination_depth_timer_->setInterval(kDepthValidationPollMilliseconds);
    connect(destination_depth_timer_, &QTimer::timeout, this, &MainApp::poll_destination_depth_validation);
    connect(destination_path_entry, &QLineEdit::textChanged, this,
            [this]() { schedule_destination_depth_validation(); });
    connect(path_entry, &QLineEdit::textChanged, this,
            [this]() { schedule_destination_depth_validation(); });
    connect(sorting_mode_selector, &QComboBox::currentIndexChanged, this,
            [this]() { schedule_destination_depth_validation(); });
    connect(categorize_files_checkbox, &QCheckBox::toggled, this,
            [this]() { schedule_destination_depth_validation(); });
    connect(categorize_directories_checkbox, &QCheckBox::toggled, this,
            [this]() { schedule_destination_depth_validation(); });
    schedule_destination_depth_validation();
}

bool MainApp::destination_depth_blocks_analysis() const
{
    if (settings.get_sorting_mode() != SortingMode::ExistingFolderTree ||
        (!settings.get_categorize_files() && !settings.get_categorize_directories())) return false;
    const auto root = get_destination_folder_path();
    return !root.empty() && (root != destination_depth_requested_root_ || !destination_depth_result_ ||
        destination_depth_result_->status != FolderTreeCatalog::DepthValidationStatus::Valid);
}

void MainApp::schedule_destination_depth_validation()
{
    if (!destination_depth_timer_) return;
    if (destination_depth_cancel_) destination_depth_cancel_->store(true);
    destination_depth_requested_root_ = settings.get_sorting_mode() == SortingMode::ExistingFolderTree &&
        (settings.get_categorize_files() || settings.get_categorize_directories())
        ? get_destination_folder_path() : std::string{};
    destination_depth_result_.reset();
    destination_depth_timer_->start();
    update_destination_depth_status();
}

void MainApp::poll_destination_depth_validation()
{
    if (destination_depth_future_.valid()) {
        if (destination_depth_future_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) return;
        FolderTreeCatalog::DepthValidationResult result;
        try {
            result = destination_depth_future_.get();
        } catch (const std::exception& ex) {
            result = {FolderTreeCatalog::DepthValidationStatus::Failed, destination_depth_running_root_, ex.what()};
        }
        if (!destination_depth_cancel_->load() && destination_depth_running_root_ == destination_depth_requested_root_) {
            destination_depth_result_ = std::move(result);
        }
    }
    if (!destination_depth_requested_root_.empty() && !destination_depth_result_) {
        destination_depth_running_root_ = destination_depth_requested_root_;
        destination_depth_cancel_ = std::make_shared<std::atomic<bool>>(false);
        const auto root = Utils::utf8_to_path(destination_depth_running_root_);
        const auto cancel = destination_depth_cancel_;
        try {
            destination_depth_future_ = std::async(std::launch::async, [root, cancel]() {
                return FolderTreeCatalog::validate_depth(root, [cancel]() { return cancel->load(); });
            });
        } catch (const std::exception& ex) {
            destination_depth_result_ = {FolderTreeCatalog::DepthValidationStatus::Failed,
                                         destination_depth_running_root_, ex.what()};
        }
    }
    if (!destination_depth_future_.valid()) destination_depth_timer_->stop();
    update_destination_depth_status();
}

void MainApp::update_destination_depth_status()
{
    if (!destination_depth_status_label) return;
    QString message;
    if (!destination_depth_requested_root_.empty()) {
        message = destination_depth_result_
            ? QString::fromStdString(FolderTreeCatalog::depth_validation_message(*destination_depth_result_))
            : tr("Checking destination folder depth...");
    }
    destination_depth_status_label->setText(message);
    destination_depth_status_label->setVisible(!message.isEmpty());
    if (destination_depth_timer_) update_analyze_button_state(analysis_in_progress_);
}
