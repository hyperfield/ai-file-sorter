#include "DryRunPreviewDialog.hpp"

#include <QHeaderView>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>

DryRunPreviewDialog::DryRunPreviewDialog(const std::vector<Entry>& entries, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Dry run preview"));
    resize(900, 480);
    setup_ui(entries);
}

void DryRunPreviewDialog::setup_ui(const std::vector<Entry>& entries)
{
    auto* layout = new QVBoxLayout(this);

    table_ = new QTableWidget(this);
    table_->setColumnCount(3);
    table_->setHorizontalHeaderLabels(QStringList{tr("From"), tr(""), tr("To")});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setAlternatingRowColors(true);

    table_->setRowCount(static_cast<int>(entries.size()));
    int row = 0;
    for (const auto& entry : entries) {
        auto* from_item = new QTableWidgetItem(QString::fromStdString(entry.from_label));
        from_item->setToolTip(QString::fromStdString(entry.source_tooltip));
        auto* arrow_item = new QTableWidgetItem(QStringLiteral("→"));
        auto* to_item = new QTableWidgetItem(QString::fromStdString(entry.to_label));
        to_item->setToolTip(QString::fromStdString(entry.destination_tooltip));
        table_->setItem(row, 0, from_item);
        table_->setItem(row, 1, arrow_item);
        table_->setItem(row, 2, to_item);
        ++row;
    }

    layout->addWidget(table_, 1);

    auto* button_layout = new QHBoxLayout();
    button_layout->addStretch(1);
    auto* close_button = new QPushButton(tr("Close"), this);
    connect(close_button, &QPushButton::clicked, this, &QDialog::accept);
    button_layout->addWidget(close_button);
    layout->addLayout(button_layout);
}
