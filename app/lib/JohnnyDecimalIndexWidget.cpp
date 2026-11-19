#include "JohnnyDecimalIndexWidget.hpp"

#include <QAbstractItemView>
#include <QDesktopServices>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QStringList>
#include <QStyle>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <utility>

#include "Utils.hpp"
#include "JohnnyDecimalIndexExport.hpp"

namespace {

constexpr int kIdColumnWidth = 150;
constexpr int kStatusColumnWidth = 220;

QString text(const std::string& value) {
    return QString::fromStdString(value);
}

bool filter_row(QTreeWidgetItem* row, const QString& query) {
    const bool matches = row->data(0, Qt::UserRole).toString().contains(query, Qt::CaseInsensitive) ||
                         row->text(0).contains(query, Qt::CaseInsensitive) ||
                         row->text(1).contains(query, Qt::CaseInsensitive) ||
                         row->text(2).contains(query, Qt::CaseInsensitive);
    bool child_matches = false;
    for (int i = 0; i < row->childCount(); ++i) {
        child_matches = filter_row(row->child(i), query) || child_matches;
    }
    row->setHidden(!matches && !child_matches);
    return !row->isHidden();
}

std::filesystem::path contained_folder(const std::filesystem::path& root, const std::string& relative) {
    std::error_code ec;
    const auto canonical_root = std::filesystem::canonical(root, ec);
    if (ec) {
        return {};
    }
    const auto target = std::filesystem::canonical(root / Utils::utf8_to_path(relative), ec);
    if (ec || !std::filesystem::is_directory(target, ec) || ec) {
        return {};
    }
    const auto within = target.lexically_relative(canonical_root);
    if (within.empty() || within.is_absolute() || *within.begin() == "..") {
        return {};
    }
    return target;
}

}  // namespace

JohnnyDecimalIndexWidget::JohnnyDecimalIndexWidget(QWidget* parent, OpenFolder open_folder)
    : QWidget(parent), open_folder_(std::move(open_folder)) {
    if (!open_folder_) {
        open_folder_ = [](const QUrl& url) { return QDesktopServices::openUrl(url); };
    }
    setObjectName(QStringLiteral("johnnyDecimalIndexWidget"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* tools = new QHBoxLayout();
    search_ = new QLineEdit(this);
    search_->setObjectName(QStringLiteral("johnnyDecimalIndexSearch"));
    search_->setPlaceholderText(tr("Search ID or name"));
    search_->setClearButtonEnabled(true);
    tools->addWidget(search_, 1);
    open_ = new QToolButton(this);
    open_->setObjectName(QStringLiteral("johnnyDecimalIndexOpen"));
    open_->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));
    open_->setToolTip(tr("Open folder"));
    open_->setAccessibleName(tr("Open folder"));
    tools->addWidget(open_);
    export_ = new QToolButton(this);
    export_->setObjectName(QStringLiteral("johnnyDecimalIndexExport"));
    export_->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    export_->setToolTip(tr("Export Markdown index"));
    export_->setAccessibleName(tr("Export Markdown index"));
    tools->addWidget(export_);
    layout->addLayout(tools);
    summary_ = new QLabel(this);
    summary_->setWordWrap(true);
    layout->addWidget(summary_);
    tree_ = new QTreeWidget(this);
    tree_->setObjectName(QStringLiteral("johnnyDecimalIndexTree"));
    tree_->setHeaderLabels({tr("ID"), tr("Folder"), tr("Status")});
    tree_->setUniformRowHeights(true);
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    tree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree_->header()->setStretchLastSection(false);
    tree_->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    tree_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    tree_->header()->setSectionResizeMode(2, QHeaderView::Interactive);
    tree_->setColumnWidth(0, kIdColumnWidth);
    tree_->setColumnWidth(2, kStatusColumnWidth);
    layout->addWidget(tree_, 1);
    message_ = new QLabel(this);
    message_->setWordWrap(true);
    message_->setObjectName(QStringLiteral("johnnyDecimalIndexMessage"));
    layout->addWidget(message_);
    connect(search_, &QLineEdit::textChanged, this, [this]() { filter(); });
    connect(tree_, &QTreeWidget::currentItemChanged, this, [this]() { update_actions(); });
    connect(tree_, &QTreeWidget::itemDoubleClicked, this, [this]() { open_selected(); });
    connect(open_, &QToolButton::clicked, this, [this]() { open_selected(); });
    connect(export_, &QToolButton::clicked, this, [this]() { export_index(); });
    update_actions();
}

void JohnnyDecimalIndexWidget::set_index(JohnnyDecimalArchiveIndex::Index index) {
    index_ = std::move(index);
    item_findings_.clear();
    for (auto& finding : JohnnyDecimalItemAnalysis::analyze(index_)) {
        item_findings_.emplace(finding.relative_path, std::move(finding));
    }
    populate();
    filter();
}

QTreeWidgetItem* JohnnyDecimalIndexWidget::add_folder(QTreeWidgetItem* parent, const std::string& id,
    const std::string& label, const std::string& relative_path, const QString& status) {
    auto* row = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree_);
    row->setText(0, text(id));
    row->setText(1, text(label));
    row->setText(2, status);
    row->setData(0, Qt::UserRole, text(relative_path));
    row->setToolTip(1, text(relative_path));
    row->setToolTip(2, status);
    row->setIcon(0, style()->standardIcon(status.isEmpty() ? QStyle::SP_DirIcon : QStyle::SP_MessageBoxWarning));
    return row;
}

QString JohnnyDecimalIndexWidget::item_status(const JohnnyDecimalItemAnalysis::Item& item) const {
    QStringList status;
    if (item.missing_id) { status << tr("Missing item ID"); }
    if (item.malformed_id) { status << tr("Malformed item ID"); }
    if (item.wrong_category) { status << tr("Item in wrong category"); }
    if (item.duplicate_id) { status << tr("Duplicate item ID"); }
    return status.join(QStringLiteral("; "));
}

void JohnnyDecimalIndexWidget::add_item(QTreeWidgetItem* parent, const JohnnyDecimalArchiveIndex::Item& item) {
    const auto finding = item_findings_.find(item.relative_path);
    const auto status = finding == item_findings_.end() ? QString() : item_status(finding->second);
    auto* row = add_folder(parent, item.id, item.label, item.relative_path, status);
    for (const auto& child : item.children) {
        add_item(row, child);
    }
}

void JohnnyDecimalIndexWidget::populate() {
    tree_->clear();
    for (const auto& area : index_.areas) {
        auto* area_row = add_folder(nullptr, area.id, area.label, area.relative_path);
        for (const auto& category : area.categories) {
            auto* category_row = add_folder(area_row, category.id, category.label, category.relative_path);
            for (const auto& item : category.items) {
                add_item(category_row, item);
            }
        }
        for (const auto& path : area.other_direct_children) {
            add_folder(area_row, {}, path.substr(path.find_last_of('/') + 1), path, tr("Unrecognized folder"));
        }
    }
    for (const auto& path : index_.other_top_level) {
        add_folder(nullptr, {}, path, path, tr("Unrecognized folder"));
    }
    constexpr int kCategoryExpansionDepth = 1;
    tree_->expandToDepth(kCategoryExpansionDepth);
    QString summary = tr("Areas: %1 | Categories: %2 | Items: %3")
        .arg(static_cast<qulonglong>(index_.areas.size()))
        .arg(static_cast<qulonglong>(JohnnyDecimalArchiveIndex::category_count(index_)))
        .arg(static_cast<qulonglong>(item_findings_.size()));
    if (index_.scan_limit_reached) {
        summary += QStringLiteral("\n") + tr("Scan limit reached; the index may be incomplete.");
    }
    summary_->setText(summary);
}

void JohnnyDecimalIndexWidget::filter() {
    bool matches = false;
    const auto query = search_->text().trimmed();
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
        matches = filter_row(tree_->topLevelItem(i), query) || matches;
    }
    if (!query.isEmpty()) {
        tree_->expandAll();
    }
    if (tree_->currentItem() && tree_->currentItem()->isHidden()) {
        tree_->setCurrentItem(nullptr);
    }
    message_->setText(!index_.scanned ? tr("Choose an existing archive folder.")
                                     : (matches ? QString() : tr("No matching folders.")));
    update_actions();
}

void JohnnyDecimalIndexWidget::update_actions() {
    open_->setEnabled(tree_->currentItem() && !tree_->currentItem()->isHidden());
    export_->setEnabled(index_.scanned);
}

void JohnnyDecimalIndexWidget::open_selected() {
    const auto* row = tree_->currentItem();
    if (!row || row->isHidden()) {
        return;
    }
    const auto folder = contained_folder(index_.root, row->data(0, Qt::UserRole).toString().toStdString());
    if (folder.empty()) {
        message_->setText(tr("Folder is unavailable or outside the archive. Refresh the index."));
        return;
    }
    if (!open_folder_(QUrl::fromLocalFile(text(Utils::path_to_utf8(folder))))) {
        message_->setText(tr("Could not open the selected folder."));
    } else {
        message_->clear();
    }
}

void JohnnyDecimalIndexWidget::export_index() {
    const QString file_name = QFileDialog::getSaveFileName(this, tr("Export Markdown index"),
        QStringLiteral("archive-index.md"), tr("Markdown files (*.md)"));
    if (file_name.isEmpty()) {
        return;
    }
    std::string error;
    if (!JohnnyDecimalIndexExport::save(index_, Utils::utf8_to_path(file_name.toStdString()), &error)) {
        message_->setText(tr("Could not save the index: %1").arg(text(error)));
        return;
    }
    message_->setText(tr("Index exported."));
}
