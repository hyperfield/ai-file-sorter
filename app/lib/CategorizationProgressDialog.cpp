#include "CategorizationProgressDialog.hpp"

#include "DocumentTextAnalyzer.hpp"
#include "LlavaImageAnalyzer.hpp"
#include "Logger.hpp"
#include "MainApp.hpp"

#include <QAbstractItemView>
#include <QColor>
#include <QEvent>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextCursor>
#include <QTimer>
#include <QVBoxLayout>
#include <QString>

#include <algorithm>

CategorizationProgressDialog::CategorizationProgressDialog(QWidget* parent,
                                                           MainApp* main_app,
                                                           bool show_subcategory_col)
    : QDialog(parent),
      main_app(main_app)
{
    resize(900, 650);
    setup_ui(show_subcategory_col);
    retranslate_ui();
}


void CategorizationProgressDialog::setup_ui(bool /*show_subcategory_col*/)
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    stage_list_label = new QLabel(this);
    stage_list_label->setTextFormat(Qt::RichText);
    layout->addWidget(stage_list_label);

    summary_label = new QLabel(this);
    layout->addWidget(summary_label);

    status_table = new QTableWidget(this);
    status_table->setColumnCount(2);
    status_table->setSelectionMode(QAbstractItemView::NoSelection);
    status_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    status_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    status_table->setAlternatingRowColors(true);
    status_table->setShowGrid(false);
    status_table->verticalHeader()->setVisible(false);
    layout->addWidget(status_table, 2);

    log_label = new QLabel(this);
    layout->addWidget(log_label);

    text_view = new QPlainTextEdit(this);
    text_view->setReadOnly(true);
    text_view->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    layout->addWidget(text_view, 1);

    auto* button_layout = new QHBoxLayout();
    button_layout->addStretch(1);

    stop_button = new QPushButton(this);
    QIcon stop_icon = QIcon::fromTheme(QStringLiteral("process-stop"));
    if (stop_icon.isNull()) {
        stop_icon = QIcon(style()->standardIcon(QStyle::SP_BrowserStop));
    }
    stop_button->setIcon(stop_icon);
    stop_button->setIconSize(QSize(18, 18));
    button_layout->addWidget(stop_button);

    layout->addLayout(button_layout);

    connect(stop_button, &QPushButton::clicked, this, &CategorizationProgressDialog::request_stop);

    spinner_timer = new QTimer(this);
    spinner_timer->setInterval(140);
    connect(spinner_timer, &QTimer::timeout, this, &CategorizationProgressDialog::refresh_spinner);
}


void CategorizationProgressDialog::show()
{
    QDialog::show();
    if (text_view) {
        text_view->moveCursor(QTextCursor::End);
    }
}


void CategorizationProgressDialog::hide()
{
    if (spinner_timer) {
        spinner_timer->stop();
    }
    QDialog::hide();
}


void CategorizationProgressDialog::append_text(const std::string& text)
{
    if (!text_view) {
        if (auto logger = Logger::get_logger("core_logger")) {
            logger->error("Progress dialog text view is null");
        }
        return;
    }

    QString qt_text = QString::fromStdString(text);
    if (!qt_text.endsWith('\n')) {
        qt_text.append('\n');
    }
    text_view->appendPlainText(qt_text);

    QScrollBar* scroll = text_view->verticalScrollBar();
    if (scroll) {
        scroll->setValue(scroll->maximum());
    }
}


void CategorizationProgressDialog::configure_stages(const std::vector<StagePlan>& stages)
{
    spinner_frame_index_ = 0;
    active_stage_order_.clear();
    active_stage_.reset();
    item_states_.clear();

    for (auto& stage_state : stage_states_) {
        stage_state.enabled = false;
        stage_state.item_keys.clear();
    }

    if (status_table) {
        status_table->setRowCount(0);
    }

    std::unordered_set<StageId> seen_stages;
    std::unordered_set<std::string> seen_items;
    std::vector<FileEntry> all_items;

    for (const auto& stage : stages) {
        if (!seen_stages.contains(stage.id)) {
            seen_stages.insert(stage.id);
            active_stage_order_.push_back(stage.id);
            stage_states_[stage_index(stage.id)].enabled = true;
        }
        auto& stage_state = stage_states_[stage_index(stage.id)];
        for (const auto& entry : stage.items) {
            const std::string key = make_item_key(entry.full_path, entry.type);
            stage_state.item_keys.insert(key);
            if (!seen_items.contains(key)) {
                seen_items.insert(key);
                all_items.push_back(entry);
            }
        }
    }

    if (!active_stage_order_.empty()) {
        active_stage_ = active_stage_order_.front();
    }

    rebuild_headers();

    for (const auto& entry : all_items) {
        upsert_item(entry);
    }

    refresh_stage_overview();
    refresh_summary();

    if (!has_in_progress_item() && spinner_timer) {
        spinner_timer->stop();
    }
}


void CategorizationProgressDialog::set_stage_items(StageId stage_id,
                                                   const std::vector<FileEntry>& items)
{
    ensure_stage_enabled(stage_id);

    auto& stage_state = stage_states_[stage_index(stage_id)];
    stage_state.item_keys.clear();

    for (const auto& entry : items) {
        const std::string key = make_item_key(entry.full_path, entry.type);
        stage_state.item_keys.insert(key);
        upsert_item(entry);
    }

    const std::size_t idx = stage_index(stage_id);
    for (auto& [key, state] : item_states_) {
        if (stage_state.item_keys.contains(key)) {
            if (state.stage_statuses[idx] == ItemStatus::NotApplicable) {
                state.stage_statuses[idx] = ItemStatus::Pending;
            }
        } else {
            state.stage_statuses[idx] = ItemStatus::NotApplicable;
        }
    }

    for (const auto& [key, state] : item_states_) {
        (void)key;
        refresh_row(state.row);
    }

    refresh_stage_overview();
    refresh_summary();
}


void CategorizationProgressDialog::set_active_stage(StageId stage_id)
{
    const auto& stage_state = stage_states_[stage_index(stage_id)];
    if (!stage_state.enabled) {
        return;
    }

    active_stage_ = stage_id;
    refresh_stage_overview();
    refresh_summary();

    if (const auto in_progress_row = find_stage_row(stage_id, ItemStatus::InProgress)) {
        ensure_row_visible(*in_progress_row);
        return;
    }
    if (const auto pending_row = find_stage_row(stage_id, ItemStatus::Pending)) {
        ensure_row_visible(*pending_row);
    }
}


void CategorizationProgressDialog::mark_stage_item_pending(StageId stage_id,
                                                           const FileEntry& entry)
{
    set_stage_item_status(stage_id, entry, ItemStatus::Pending);
}


void CategorizationProgressDialog::mark_stage_item_in_progress(StageId stage_id,
                                                               const FileEntry& entry)
{
    set_stage_item_status(stage_id, entry, ItemStatus::InProgress);
}


void CategorizationProgressDialog::mark_stage_item_completed(StageId stage_id,
                                                             const FileEntry& entry)
{
    set_stage_item_status(stage_id, entry, ItemStatus::Completed);
}


void CategorizationProgressDialog::request_stop()
{
    if (!main_app) {
        return;
    }
    main_app->report_progress(
        tr("[STOP] Analysis will stop after the current item is processed.")
            .toUtf8()
            .toStdString());
    main_app->request_stop_analysis();
}


std::string CategorizationProgressDialog::make_item_key(const std::string& full_path,
                                                        FileType type)
{
    return full_path + "|" + std::to_string(static_cast<int>(type));
}


std::size_t CategorizationProgressDialog::stage_index(StageId stage_id)
{
    switch (stage_id) {
        case StageId::ImageAnalysis:
            return 0;
        case StageId::DocumentAnalysis:
            return 1;
        case StageId::Categorization:
        default:
            return 2;
    }
}


QString CategorizationProgressDialog::stage_label(StageId stage_id) const
{
    switch (stage_id) {
        case StageId::ImageAnalysis:
            return tr("Image analysis");
        case StageId::DocumentAnalysis:
            return tr("Document analysis");
        case StageId::Categorization:
        default:
            return tr("Categorization");
    }
}


CategorizationProgressDialog::DisplayType CategorizationProgressDialog::classify_display_type(const FileEntry& entry)
{
    if (entry.type == FileType::Directory) {
        return DisplayType::Directory;
    }
    if (LlavaImageAnalyzer::is_supported_image(entry.full_path)) {
        return DisplayType::Image;
    }
    if (DocumentTextAnalyzer::is_supported_document(entry.full_path)) {
        return DisplayType::Document;
    }
    return DisplayType::File;
}


QString CategorizationProgressDialog::display_type_label(DisplayType display_type) const
{
    switch (display_type) {
        case DisplayType::Directory:
            return tr("Directory");
        case DisplayType::Image:
            return tr("Image");
        case DisplayType::Document:
            return tr("Document");
        case DisplayType::File:
        default:
            return tr("File");
    }
}


int CategorizationProgressDialog::column_for_stage(StageId stage_id) const
{
    for (std::size_t i = 0; i < active_stage_order_.size(); ++i) {
        if (active_stage_order_[i] == stage_id) {
            return static_cast<int>(2 + i);
        }
    }
    return -1;
}


void CategorizationProgressDialog::ensure_stage_enabled(StageId stage_id)
{
    auto& stage_state = stage_states_[stage_index(stage_id)];
    if (!stage_state.enabled) {
        stage_state.enabled = true;
        if (std::find(active_stage_order_.begin(), active_stage_order_.end(), stage_id) == active_stage_order_.end()) {
            active_stage_order_.push_back(stage_id);
        }
    }

    if (!active_stage_.has_value()) {
        active_stage_ = stage_id;
    }

    rebuild_headers();
}


void CategorizationProgressDialog::upsert_stage_item(StageId stage_id,
                                                     const FileEntry& entry)
{
    ensure_stage_enabled(stage_id);

    const std::string key = make_item_key(entry.full_path, entry.type);
    auto& stage_state = stage_states_[stage_index(stage_id)];
    stage_state.item_keys.insert(key);

    upsert_item(entry);

    auto it = item_states_.find(key);
    if (it == item_states_.end()) {
        return;
    }

    const std::size_t idx = stage_index(stage_id);
    if (it->second.stage_statuses[idx] == ItemStatus::NotApplicable) {
        it->second.stage_statuses[idx] = ItemStatus::Pending;
    }
}


void CategorizationProgressDialog::upsert_item(const FileEntry& entry)
{
    if (!status_table) {
        return;
    }

    const std::string key = make_item_key(entry.full_path, entry.type);
    if (item_states_.contains(key)) {
        return;
    }

    const int row = status_table->rowCount();
    status_table->insertRow(row);

    auto* file_item = new QTableWidgetItem(QString::fromStdString(entry.file_name));
    file_item->setToolTip(QString::fromStdString(entry.full_path));
    status_table->setItem(row, 0, file_item);

    auto* type_item = new QTableWidgetItem(display_type_label(classify_display_type(entry)));
    type_item->setTextAlignment(Qt::AlignCenter);
    status_table->setItem(row, 1, type_item);

    ItemState state;
    state.row = row;
    state.display_type = classify_display_type(entry);

    for (std::size_t i = 0; i < kStageCount; ++i) {
        const auto& stage_state = stage_states_[i];
        if (stage_state.enabled && stage_state.item_keys.contains(key)) {
            state.stage_statuses[i] = ItemStatus::Pending;
        } else {
            state.stage_statuses[i] = ItemStatus::NotApplicable;
        }
    }

    item_states_.emplace(key, state);
    refresh_row(row);
}


void CategorizationProgressDialog::set_stage_item_status(StageId stage_id,
                                                         const FileEntry& entry,
                                                         ItemStatus status)
{
    upsert_stage_item(stage_id, entry);

    const std::string key = make_item_key(entry.full_path, entry.type);
    auto it = item_states_.find(key);
    if (it == item_states_.end()) {
        return;
    }

    ItemState& state = it->second;
    const std::size_t idx = stage_index(stage_id);

    if (state.stage_statuses[idx] == status) {
        if (status == ItemStatus::InProgress) {
            refresh_row(state.row);
            ensure_row_visible(state.row);
        }
        return;
    }

    state.stage_statuses[idx] = status;

    refresh_row(state.row);
    refresh_summary();

    if (!spinner_timer) {
        if (status == ItemStatus::InProgress) {
            ensure_row_visible(state.row);
        }
        return;
    }
    if (has_in_progress_item()) {
        if (!spinner_timer->isActive()) {
            spinner_timer->start();
        }
    } else {
        spinner_timer->stop();
    }

    if (status == ItemStatus::InProgress) {
        ensure_row_visible(state.row);
    }
}


void CategorizationProgressDialog::rebuild_headers()
{
    if (!status_table) {
        return;
    }

    const int stage_col_count = static_cast<int>(active_stage_order_.size());
    status_table->setColumnCount(2 + stage_col_count);

    QStringList headers;
    headers << tr("File") << tr("Type");
    for (StageId stage_id : active_stage_order_) {
        headers << stage_label(stage_id);
    }
    status_table->setHorizontalHeaderLabels(headers);

    auto* header = status_table->horizontalHeader();
    if (!header) {
        return;
    }
    header->setStretchLastSection(false);
    header->setSectionResizeMode(0, QHeaderView::Stretch);
    header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    for (int col = 2; col < status_table->columnCount(); ++col) {
        header->setSectionResizeMode(col, QHeaderView::ResizeToContents);
    }
}


void CategorizationProgressDialog::refresh_stage_overview()
{
    if (!stage_list_label) {
        return;
    }

    if (active_stage_order_.empty()) {
        stage_list_label->setText(QString());
        return;
    }

    QStringList lines;
    for (std::size_t i = 0; i < active_stage_order_.size(); ++i) {
        const StageId stage_id = active_stage_order_[i];
        QString line = tr("Stage %1: %2")
                           .arg(static_cast<int>(i + 1))
                           .arg(stage_label(stage_id));
        if (active_stage_.has_value() && active_stage_.value() == stage_id) {
            line = QStringLiteral("<b>%1</b>").arg(line);
        }
        lines << line;
    }
    stage_list_label->setText(lines.join(QStringLiteral("<br/>")));
}


CategorizationProgressDialog::ItemStatus CategorizationProgressDialog::stage_status_for_row(
    const ItemState& state,
    StageId stage_id) const
{
    return state.stage_statuses[stage_index(stage_id)];
}


std::optional<int> CategorizationProgressDialog::find_stage_row(StageId stage_id,
                                                                ItemStatus status) const
{
    std::optional<int> best_row;
    const std::size_t idx = stage_index(stage_id);

    for (const auto& [key, item] : item_states_) {
        (void)key;
        if (item.stage_statuses[idx] != status) {
            continue;
        }
        if (!best_row.has_value() || item.row < *best_row) {
            best_row = item.row;
        }
    }

    return best_row;
}


void CategorizationProgressDialog::ensure_row_visible(int row)
{
    if (!status_table || row < 0 || row >= status_table->rowCount()) {
        return;
    }

    auto* anchor_item = status_table->item(row, 0);
    if (!anchor_item) {
        return;
    }

    status_table->scrollToItem(anchor_item, QAbstractItemView::EnsureVisible);
}


void CategorizationProgressDialog::refresh_row(int row)
{
    if (!status_table || row < 0 || row >= status_table->rowCount()) {
        return;
    }

    const ItemState* state = nullptr;
    for (const auto& [key, item] : item_states_) {
        (void)key;
        if (item.row == row) {
            state = &item;
            break;
        }
    }
    if (!state) {
        return;
    }

    auto* type_item = status_table->item(row, 1);
    if (!type_item) {
        type_item = new QTableWidgetItem();
        status_table->setItem(row, 1, type_item);
    }
    type_item->setText(display_type_label(state->display_type));
    type_item->setTextAlignment(Qt::AlignCenter);

    static const QString kSpinnerFrames[] = {
        QStringLiteral("◐"),
        QStringLiteral("◓"),
        QStringLiteral("◑"),
        QStringLiteral("◒")
    };

    for (StageId stage_id : active_stage_order_) {
        const int col = column_for_stage(stage_id);
        if (col < 0) {
            continue;
        }

        auto* stage_item = status_table->item(row, col);
        if (!stage_item) {
            stage_item = new QTableWidgetItem();
            status_table->setItem(row, col, stage_item);
        }

        const ItemStatus status = stage_status_for_row(*state, stage_id);
        switch (status) {
            case ItemStatus::NotApplicable:
                stage_item->setText(QStringLiteral("—"));
                stage_item->setForeground(QColor(150, 150, 150));
                break;
            case ItemStatus::Pending:
                stage_item->setText(QStringLiteral("○ %1").arg(tr("Pending")));
                stage_item->setForeground(QColor(120, 120, 120));
                break;
            case ItemStatus::InProgress:
                stage_item->setText(QStringLiteral("%1 %2")
                                        .arg(kSpinnerFrames[spinner_frame_index_ % 4], tr("In progress")));
                stage_item->setForeground(QColor(33, 150, 243));
                break;
            case ItemStatus::Completed:
                stage_item->setText(QStringLiteral("✓ %1").arg(tr("Complete")));
                stage_item->setForeground(QColor(46, 125, 50));
                break;
        }

        stage_item->setTextAlignment(Qt::AlignCenter);
    }
}


void CategorizationProgressDialog::refresh_summary()
{
    if (!summary_label) {
        return;
    }

    if (!active_stage_.has_value()) {
        summary_label->setText(tr("Processed 0/0  |  In progress: 0  |  Pending: 0"));
        return;
    }

    const StageId current_stage = active_stage_.value();
    const auto& stage_state = stage_states_[stage_index(current_stage)];

    int processed = 0;
    int in_progress = 0;
    int pending = 0;

    for (const auto& key : stage_state.item_keys) {
        auto it = item_states_.find(key);
        if (it == item_states_.end()) {
            continue;
        }

        const ItemStatus status = it->second.stage_statuses[stage_index(current_stage)];
        switch (status) {
            case ItemStatus::Completed:
                ++processed;
                break;
            case ItemStatus::InProgress:
                ++in_progress;
                break;
            case ItemStatus::Pending:
                ++pending;
                break;
            case ItemStatus::NotApplicable:
            default:
                break;
        }
    }

    const int total = static_cast<int>(stage_state.item_keys.size());
    summary_label->setText(tr("Processed %1/%2  |  In progress: %3  |  Pending: %4")
                               .arg(processed)
                               .arg(total)
                               .arg(in_progress)
                               .arg(pending));
}


void CategorizationProgressDialog::refresh_spinner()
{
    if (!has_in_progress_item()) {
        if (spinner_timer) {
            spinner_timer->stop();
        }
        return;
    }

    spinner_frame_index_ = (spinner_frame_index_ + 1) % 4;

    for (const auto& [key, item] : item_states_) {
        (void)key;
        for (StageId stage_id : active_stage_order_) {
            if (item.stage_statuses[stage_index(stage_id)] == ItemStatus::InProgress) {
                refresh_row(item.row);
                break;
            }
        }
    }
}


bool CategorizationProgressDialog::has_in_progress_item() const
{
    for (const auto& [key, item] : item_states_) {
        (void)key;
        for (StageId stage_id : active_stage_order_) {
            if (item.stage_statuses[stage_index(stage_id)] == ItemStatus::InProgress) {
                return true;
            }
        }
    }
    return false;
}


void CategorizationProgressDialog::changeEvent(QEvent* event)
{
    QDialog::changeEvent(event);
    if (event && event->type() == QEvent::LanguageChange) {
        retranslate_ui();
    }
}


void CategorizationProgressDialog::retranslate_ui()
{
    setWindowTitle(tr("Analyzing Files"));
    if (stop_button) {
        stop_button->setText(tr("Stop Analysis"));
    }
    if (log_label) {
        log_label->setText(tr("Activity log"));
    }

    rebuild_headers();
    refresh_stage_overview();

    for (const auto& [key, item] : item_states_) {
        (void)key;
        refresh_row(item.row);
    }
    refresh_summary();
}
