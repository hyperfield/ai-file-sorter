#include "ReviewHistoryDialog.hpp"

#include "StorageProvider.hpp"

#include <QAbstractItemView>
#include <QBrush>
#include <QDateTime>
#include <QEvent>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QModelIndex>
#include <QPushButton>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>

namespace {

QString elided_cell_text(const std::string& value, int max_chars = 180)
{
    QString text = QString::fromStdString(value);
    if (text.size() <= max_chars) {
        return text;
    }
    return text.left(max_chars - 1) + QStringLiteral("…");
}

QStandardItem* make_read_only_item(const QString& text, const QString& tooltip = {})
{
    auto* item = new QStandardItem(text);
    item->setEditable(false);
    if (!tooltip.isEmpty()) {
        item->setToolTip(tooltip);
    }
    return item;
}

} // namespace

ReviewHistoryDialog::ReviewHistoryDialog(ReviewHistoryStore& store,
                                         IStorageProvider& storage_provider,
                                         QWidget* parent)
    : QDialog(parent),
      store_(store),
      storage_provider_(storage_provider)
{
    resize(1100, 640);
    setSizeGripEnabled(true);
    setup_ui();
    retranslate_ui();
    load_entries();
}

void ReviewHistoryDialog::changeEvent(QEvent* event)
{
    if (event && event->type() == QEvent::LanguageChange) {
        retranslate_ui();
        load_entries();
    }
    QDialog::changeEvent(event);
}

void ReviewHistoryDialog::setup_ui()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto* search_row = new QHBoxLayout();
    search_row->setContentsMargins(0, 0, 0, 0);
    search_row->setSpacing(8);
    auto* search_label = new QLabel(this);
    search_label->setObjectName(QStringLiteral("reviewHistorySearchLabel"));
    search_edit_ = new QLineEdit(this);
    search_edit_->setObjectName(QStringLiteral("reviewHistorySearchEdit"));
    search_label->setBuddy(search_edit_);
    search_row->addWidget(search_label);
    search_row->addWidget(search_edit_, 1);
    layout->addLayout(search_row);

    model_ = new QStandardItemModel(this);
    model_->setColumnCount(ColumnCount);

    table_view_ = new QTableView(this);
    table_view_->setObjectName(QStringLiteral("reviewHistoryTable"));
    table_view_->setModel(model_);
    table_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_view_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_view_->setAlternatingRowColors(true);
    table_view_->setSortingEnabled(false);
    table_view_->verticalHeader()->setVisible(false);
    table_view_->horizontalHeader()->setStretchLastSection(false);
    table_view_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table_view_->setColumnWidth(ColumnDate, 145);
    table_view_->setColumnWidth(ColumnOperation, 160);
    table_view_->setColumnWidth(ColumnOriginalName, 180);
    table_view_->setColumnWidth(ColumnFinalName, 180);
    table_view_->setColumnWidth(ColumnCategory, 180);
    table_view_->setColumnWidth(ColumnDescription, 280);
    table_view_->setColumnWidth(ColumnStatus, 90);
    table_view_->setColumnWidth(ColumnSource, 240);
    table_view_->setColumnWidth(ColumnDestination, 240);
    layout->addWidget(table_view_, 1);

    auto* button_row = new QHBoxLayout();
    button_row->setContentsMargins(0, 0, 0, 0);
    button_row->setSpacing(8);
    button_row->addStretch(1);
    undo_button_ = new QPushButton(this);
    undo_button_->setEnabled(false);
    close_button_ = new QPushButton(this);
    button_row->addWidget(undo_button_);
    button_row->addWidget(close_button_);
    layout->addLayout(button_row);

    connect(search_edit_, &QLineEdit::textChanged, this, [this]() {
        load_entries();
    });
    connect(close_button_, &QPushButton::clicked, this, &QDialog::accept);
    connect(undo_button_, &QPushButton::clicked, this, [this]() {
        undo_selected_entries();
    });
    if (table_view_->selectionModel()) {
        connect(table_view_->selectionModel(),
                &QItemSelectionModel::selectionChanged,
                this,
                [this]() { update_undo_button_state(); });
    }
}

void ReviewHistoryDialog::retranslate_ui()
{
    setWindowTitle(tr("Rename and Categorization History"));
    if (auto* label = findChild<QLabel*>(QStringLiteral("reviewHistorySearchLabel"))) {
        label->setText(tr("Search:"));
    }
    if (search_edit_) {
        search_edit_->setPlaceholderText(
            tr("Search filenames, categories, descriptions, or paths"));
        search_edit_->setAccessibleName(tr("Search review history"));
    }
    if (model_) {
        model_->setHorizontalHeaderLabels(QStringList{
            tr("Date"),
            tr("Operation"),
            tr("Original filename"),
            tr("New filename"),
            tr("Category"),
            tr("Description"),
            tr("Status"),
            tr("Original path"),
            tr("New path")
        });
    }
    if (undo_button_) {
        undo_button_->setText(tr("Undo selected"));
    }
    if (close_button_) {
        close_button_->setText(tr("Close"));
    }
}

void ReviewHistoryDialog::load_entries()
{
    if (!model_) {
        return;
    }
    model_->removeRows(0, model_->rowCount());
    const std::string query = search_edit_ ? search_edit_->text().trimmed().toStdString() : std::string();
    for (const auto& entry : store_.search_entries(query)) {
        append_entry_row(entry);
    }
    update_undo_button_state();
}

void ReviewHistoryDialog::append_entry_row(const ReviewHistoryStore::Entry& entry)
{
    QList<QStandardItem*> row;
    const QString source = QString::fromStdString(entry.source_path);
    const QString destination = QString::fromStdString(entry.destination_path);
    const QString description = QString::fromStdString(entry.file_description);

    row << make_read_only_item(display_date(entry.created_at_utc))
        << make_read_only_item(display_operation(entry.operation))
        << make_read_only_item(QString::fromStdString(entry.original_file_name), source)
        << make_read_only_item(QString::fromStdString(entry.final_file_name), destination)
        << make_read_only_item(display_category(entry))
        << make_read_only_item(elided_cell_text(entry.file_description), description)
        << make_read_only_item(entry.undone ? tr("Undone") : tr("Applied"))
        << make_read_only_item(elided_cell_text(entry.source_path, 220), source)
        << make_read_only_item(elided_cell_text(entry.destination_path, 220), destination);

    for (auto* item : row) {
        item->setData(static_cast<qlonglong>(entry.id), kEntryIdRole);
        item->setData(entry.undone, kUndoneRole);
        if (entry.undone) {
            item->setForeground(QBrush(Qt::gray));
        }
    }
    model_->appendRow(row);
}

void ReviewHistoryDialog::undo_selected_entries()
{
    const auto ids = selected_entry_ids();
    if (ids.empty()) {
        QMessageBox::information(this,
                                 tr("No history items selected"),
                                 tr("Select one or more history rows to undo."));
        return;
    }

    const QString prompt = ids.size() == 1
        ? tr("Undo the selected history item?")
        : tr("Undo %1 selected history items?").arg(ids.size());
    if (QMessageBox::question(this,
                              tr("Undo selected history"),
                              prompt,
                              QMessageBox::Ok | QMessageBox::Cancel,
                              QMessageBox::Cancel) != QMessageBox::Ok) {
        return;
    }

    int restored = 0;
    int skipped = 0;
    QStringList details;

    for (const long long id : ids) {
        const auto entry = store_.entry_by_id(id);
        if (!entry) {
            ++skipped;
            details << tr("Missing history row: %1").arg(id);
            continue;
        }
        if (entry->undone) {
            ++skipped;
            details << tr("Already undone: %1").arg(QString::fromStdString(entry->final_file_name));
            continue;
        }
        QStringList validation_details;
        if (!can_undo_entry(*entry, validation_details)) {
            ++skipped;
            details << validation_details;
            continue;
        }

        const auto result = storage_provider_.undo_move(entry->source_path, entry->destination_path);
        if (!result.success) {
            ++skipped;
            details << QString::fromStdString(
                result.message.empty() ? "Undo failed." : result.message);
            continue;
        }

        std::string error;
        if (!store_.mark_undone(id, &error)) {
            details << tr("Restored but could not update history row %1: %2")
                           .arg(id)
                           .arg(QString::fromStdString(error));
        }
        mark_row_undone(id);
        ++restored;
    }

    QString summary = tr("Restored %1 item(s). Skipped %2.").arg(restored).arg(skipped);
    if (!details.isEmpty()) {
        summary.append(QStringLiteral("\n"));
        summary.append(details.join(QStringLiteral("\n")));
    }
    QMessageBox::information(this, tr("Undo complete"), summary);
    update_undo_button_state();
}

void ReviewHistoryDialog::update_undo_button_state()
{
    if (!undo_button_ || !table_view_ || !table_view_->selectionModel() || !model_) {
        return;
    }

    bool can_undo = false;
    const auto rows = table_view_->selectionModel()->selectedRows();
    for (const QModelIndex& index : rows) {
        const auto* item = model_->item(index.row(), ColumnDate);
        if (item && !item->data(kUndoneRole).toBool()) {
            can_undo = true;
            break;
        }
    }
    undo_button_->setEnabled(can_undo);
}

void ReviewHistoryDialog::mark_row_undone(long long id)
{
    if (!model_) {
        return;
    }
    for (int row = 0; row < model_->rowCount(); ++row) {
        auto* id_item = model_->item(row, ColumnDate);
        if (!id_item || id_item->data(kEntryIdRole).toLongLong() != id) {
            continue;
        }
        for (int col = 0; col < model_->columnCount(); ++col) {
            if (auto* item = model_->item(row, col)) {
                item->setData(true, kUndoneRole);
                item->setForeground(QBrush(Qt::gray));
            }
        }
        if (auto* status_item = model_->item(row, ColumnStatus)) {
            status_item->setText(tr("Undone"));
        }
        return;
    }
}

std::vector<long long> ReviewHistoryDialog::selected_entry_ids() const
{
    std::vector<long long> ids;
    if (!table_view_ || !table_view_->selectionModel() || !model_) {
        return ids;
    }
    const auto rows = table_view_->selectionModel()->selectedRows();
    ids.reserve(static_cast<std::size_t>(rows.size()));
    for (const QModelIndex& index : rows) {
        const auto* item = model_->item(index.row(), ColumnDate);
        if (!item) {
            continue;
        }
        const long long id = item->data(kEntryIdRole).toLongLong();
        if (id > 0 && std::find(ids.begin(), ids.end(), id) == ids.end()) {
            ids.push_back(id);
        }
    }
    return ids;
}

QString ReviewHistoryDialog::display_operation(ReviewHistoryStore::Operation operation) const
{
    switch (operation) {
    case ReviewHistoryStore::Operation::Rename:
        return tr("Rename");
    case ReviewHistoryStore::Operation::RenameAndCategorize:
        return tr("Rename and categorize");
    case ReviewHistoryStore::Operation::Categorize:
    default:
        return tr("Categorize");
    }
}

QString ReviewHistoryDialog::display_category(const ReviewHistoryStore::Entry& entry) const
{
    if (entry.category.empty()) {
        return QStringLiteral("-");
    }
    if (entry.subcategory.empty() || entry.subcategory == entry.category) {
        return QString::fromStdString(entry.category);
    }
    return QString::fromStdString(entry.category + " / " + entry.subcategory);
}

QString ReviewHistoryDialog::display_date(const std::string& created_at_utc) const
{
    QDateTime date = QDateTime::fromString(QString::fromStdString(created_at_utc), Qt::ISODate);
    if (!date.isValid()) {
        date = QDateTime::fromString(QString::fromStdString(created_at_utc), Qt::ISODateWithMs);
    }
    if (!date.isValid()) {
        return QString::fromStdString(created_at_utc);
    }
    return QLocale().toString(date.toLocalTime(), QLocale::ShortFormat);
}

bool ReviewHistoryDialog::can_undo_entry(const ReviewHistoryStore::Entry& entry,
                                         QStringList& details) const
{
    QFileInfo destination_info(QString::fromStdString(entry.destination_path));
    if (!destination_info.exists()) {
        details << tr("Missing destination: %1").arg(QString::fromStdString(entry.destination_path));
        return false;
    }

    QFileInfo source_info(QString::fromStdString(entry.source_path));
    if (source_info.exists()) {
        details << tr("Original path already exists: %1").arg(QString::fromStdString(entry.source_path));
        return false;
    }

    if (entry.size_bytes > 0 &&
        destination_info.size() != static_cast<qint64>(entry.size_bytes)) {
        details << tr("Size mismatch: %1").arg(QString::fromStdString(entry.destination_path));
        return false;
    }

    const auto capabilities = storage_provider_.capabilities();
    if (entry.mtime > 0 && !capabilities.should_relax_undo_mtime_validation) {
        const auto mtime = destination_info.lastModified().toSecsSinceEpoch();
        if (mtime != entry.mtime) {
            details << tr("Timestamp mismatch: %1").arg(QString::fromStdString(entry.destination_path));
            return false;
        }
    }

    const auto status = storage_provider_.inspect_path(entry.destination_path);
    if (!entry.stable_identity.empty() &&
        !status.stable_identity.empty() &&
        status.stable_identity != entry.stable_identity) {
        details << tr("Identity mismatch: %1").arg(QString::fromStdString(entry.destination_path));
        return false;
    }
    if (!entry.revision_token.empty() &&
        !status.revision_token.empty() &&
        status.revision_token != entry.revision_token) {
        details << tr("Revision mismatch: %1").arg(QString::fromStdString(entry.destination_path));
        return false;
    }
    return true;
}
