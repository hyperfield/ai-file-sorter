#include "CategorizationDialog.hpp"

#include "AppTheme.hpp"
#include "BulkEditDialog.hpp"
#include "DatabaseManager.hpp"
#include "LocalFsProvider.hpp"
#include "Logger.hpp"
#include "MovableCategorizedFile.hpp"
#include "ReviewNameValidator.hpp"
#include "TestHooks.hpp"
#include "Utils.hpp"
#include "UndoManager.hpp"
#include "UserLearningStore.hpp"
#include "DryRunPreviewDialog.hpp"
#include "IFilePreviewService.hpp"
#include "ReviewFileNaming.hpp"
#include "ReviewHistoryStore.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QStyle>
#include <QBrush>
#include <QCheckBox>
#include <QCloseEvent>
#include <QEvent>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QPushButton>
#include <QIcon>
#include <QImage>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMessageBox>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringList>
#include <QShortcut>
#include <QTableView>
#include <QVBoxLayout>
#include <QSignalBlocker>
#include <QFile>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>
#include <QBuffer>
#include <QScrollArea>
#include <QScopedValueRollback>
#include <QScreen>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>

#include <fmt/format.h>

#include <algorithm>
#include <vector>
#include <filesystem>
#include <optional>
#include <chrono>
#include <unordered_map>
#include <unordered_set>

namespace {

TestHooks::CategorizationMoveProbe& move_probe_slot() {
    static TestHooks::CategorizationMoveProbe probe;
    return probe;
}

IStorageProvider& default_storage_provider()
{
    static LocalFsProvider provider;
    return provider;
}

struct ScopedFlag {
    bool& ref;
    explicit ScopedFlag(bool& target) : ref(target) { ref = true; }
    ~ScopedFlag() { ref = false; }
};

QString edit_icon_html(int size = 16);

using ReviewNameValidator::is_missing_category_label;
using ReviewNameValidator::strip_history_description_label;
using ReviewNameValidator::trim_copy;
using ReviewNameValidator::validate_labels;
using ReviewFileNaming::build_suggested_target_dir;
using ReviewFileNaming::build_unique_move_name;
using ReviewFileNaming::ensure_unique_suggested_names;
using ReviewFileNaming::is_supported_document_entry;
using ReviewFileNaming::is_supported_image_entry;
using ReviewFileNaming::to_lower_copy_str;

std::string read_item_or_hidden_text(const QStandardItem* item, int hidden_role)
{
    if (!item) {
        return std::string();
    }

    const std::string text = item->text().toStdString();
    if (!text.empty()) {
        return text;
    }

    if (hidden_role >= 0 && item->data(hidden_role).isValid()) {
        return item->data(hidden_role).toString().toStdString();
    }

    return std::string();
}

} // namespace

namespace TestHooks {

void set_categorization_move_probe(CategorizationMoveProbe probe) {
    move_probe_slot() = std::move(probe);
}

void reset_categorization_move_probe() {
    move_probe_slot() = CategorizationMoveProbe{};
}

} // namespace TestHooks

CategorizationDialog::CategorizationDialog(DatabaseManager* db_manager,
                                           bool show_subcategory_col,
                                           const std::string& undo_dir,
                                           CategoryLanguage category_language,
                                           QWidget* parent,
                                           UserLearningStore* learning_store,
                                           ReviewHistoryStore* history_store)
    : CategorizationDialog(db_manager,
                           default_storage_provider(),
                           show_subcategory_col,
                           undo_dir,
                           category_language,
                           parent,
                           learning_store,
                           history_store)
{
}

CategorizationDialog::CategorizationDialog(DatabaseManager* db_manager,
                                           IStorageProvider& storage_provider,
                                           bool show_subcategory_col,
                                           const std::string& undo_dir,
                                           CategoryLanguage category_language,
                                           QWidget* parent,
                                           UserLearningStore* learning_store,
                                           ReviewHistoryStore* history_store)
    : QDialog(parent),
      db_manager(db_manager),
      learning_store_(learning_store),
      history_store_(history_store),
      storage_provider_(&storage_provider),
      category_language_(category_language),
      show_subcategory_column(show_subcategory_col),
      core_logger(Logger::get_logger("core_logger")),
      db_logger(Logger::get_logger("db_logger")),
      ui_logger(Logger::get_logger("ui_logger")),
      undo_dir_(undo_dir),
      preview_service_(create_file_preview_service())
{
    resize(1100, 720);
    setSizeGripEnabled(true);
    Qt::WindowFlags flags = windowFlags();
    flags &= ~Qt::WindowType_Mask;
    flags |= Qt::Window;
    flags |= Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint;
    flags &= ~Qt::MSWindowsFixedSizeDialogHint;
    setWindowFlags(flags);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    setup_ui();
    retranslate_ui();
}

CategorizationDialog::~CategorizationDialog() = default;

void CategorizationDialog::set_file_preview_service(std::unique_ptr<IFilePreviewService> preview_service)
{
    preview_service_ = preview_service ? std::move(preview_service)
                                       : create_file_preview_service();
}

void CategorizationDialog::set_review_auto_approve_filename_changes_enabled(bool enabled)
{
    auto_approve_filename_changes_ = enabled;
    if (auto_approve_filenames_checkbox) {
        QSignalBlocker blocker(auto_approve_filenames_checkbox);
        auto_approve_filenames_checkbox->setChecked(enabled);
    }
    if ((auto_approve_filename_changes_ || auto_approve_categorization_) &&
        model && model->rowCount() > 0) {
        apply_auto_approval_to_rows();
    }
}

bool CategorizationDialog::review_auto_approve_filename_changes_enabled() const
{
    return auto_approve_filenames_checkbox
        ? auto_approve_filenames_checkbox->isChecked()
        : auto_approve_filename_changes_;
}

void CategorizationDialog::set_review_auto_approve_categorization_enabled(bool enabled)
{
    auto_approve_categorization_ = enabled;
    if (auto_approve_categories_checkbox) {
        QSignalBlocker blocker(auto_approve_categories_checkbox);
        auto_approve_categories_checkbox->setChecked(enabled);
    }
    if ((auto_approve_filename_changes_ || auto_approve_categorization_) &&
        model && model->rowCount() > 0) {
        apply_auto_approval_to_rows();
    }
}

bool CategorizationDialog::review_auto_approve_categorization_enabled() const
{
    return auto_approve_categories_checkbox
        ? auto_approve_categories_checkbox->isChecked()
        : auto_approve_categorization_;
}


bool CategorizationDialog::is_dialog_valid() const
{
    return model != nullptr && table_view != nullptr;
}


void CategorizationDialog::show_results(const std::vector<CategorizedFile>& files,
                                        const std::string& base_dir_override,
                                        bool include_subdirectories,
                                        bool allow_image_renames,
                                        bool allow_document_renames,
                                        bool auto_approve_filename_changes,
                                        bool auto_approve_categorization)
{
    categorized_files = files;
    include_subdirectories_ = include_subdirectories;
    allow_image_renames_ = allow_image_renames;
    allow_document_renames_ = allow_document_renames;
    auto_approve_filename_changes_ = auto_approve_filename_changes;
    auto_approve_categorization_ = auto_approve_categorization;
    if (auto_approve_filenames_checkbox) {
        QSignalBlocker blocker(auto_approve_filenames_checkbox);
        auto_approve_filenames_checkbox->setChecked(auto_approve_filename_changes_);
    }
    if (auto_approve_categories_checkbox) {
        QSignalBlocker blocker(auto_approve_categories_checkbox);
        auto_approve_categories_checkbox->setChecked(auto_approve_categorization_);
    }
    base_dir_.clear();
    if (!base_dir_override.empty()) {
        base_dir_ = base_dir_override;
    } else if (!categorized_files.empty()) {
        base_dir_ = categorized_files.front().file_path;
    }
    ensure_unique_suggested_names(categorized_files, base_dir_, show_subcategory_column);
    set_show_rename_column(std::any_of(categorized_files.begin(),
                                       categorized_files.end(),
                                       [](const CategorizedFile& file) {
                                           return !file.suggested_name.empty();
                                       }));
    update_rename_only_checkbox_state();
    apply_category_visibility();
    apply_subcategory_visibility();
    dry_run_plan_.clear();
    clear_move_history();
    if (undo_button) {
        undo_button->setEnabled(false);
        undo_button->setVisible(false);
    }
    {
        ScopedFlag guard(suppress_item_changed_);
        populate_model();
    }
    const bool auto_approve_filenames_after_init = auto_approve_filename_changes_;
    const bool auto_approve_categories_after_init = auto_approve_categorization_;
    auto_approve_filename_changes_ = false;
    auto_approve_categorization_ = false;
    on_rename_images_only_toggled(rename_images_only_checkbox && rename_images_only_checkbox->isChecked());
    on_rename_documents_only_toggled(rename_documents_only_checkbox && rename_documents_only_checkbox->isChecked());
    auto_approve_filename_changes_ = auto_approve_filenames_after_init;
    auto_approve_categorization_ = auto_approve_categories_after_init;
    update_subcategory_checkbox_state();
    if (auto_approve_filename_changes_ || auto_approve_categorization_) {
        apply_auto_approval_to_rows();
    }
    exec();
}


void CategorizationDialog::setup_ui()
{
    apply_theme_styles();

    auto* layout = new QVBoxLayout(this);

    auto* scroll_area = new QScrollArea(this);
    scroll_area->setWidgetResizable(true);
    scroll_area->setFrameShape(QFrame::NoFrame);

    auto* scroll_widget = new QWidget(scroll_area);
    auto* scroll_layout = new QVBoxLayout(scroll_widget);
    scroll_layout->setContentsMargins(0, 0, 0, 0);

    auto* select_layout = new QHBoxLayout();
    select_layout->setContentsMargins(0, 0, 0, 0);

    select_all_checkbox = new QCheckBox(this);
    select_all_checkbox->setChecked(true);
    auto_approve_filenames_checkbox = new QCheckBox(this);
    auto_approve_filenames_checkbox->setChecked(auto_approve_filename_changes_);
    auto_approve_categories_checkbox = new QCheckBox(this);
    auto_approve_categories_checkbox->setChecked(auto_approve_categorization_);
    select_highlighted_button = new QPushButton(this);
    bulk_edit_button = new QPushButton(this);

    select_layout->addWidget(select_all_checkbox);
    select_layout->addWidget(auto_approve_filenames_checkbox);
    select_layout->addWidget(auto_approve_categories_checkbox);
    select_layout->addWidget(select_highlighted_button);
    select_layout->addWidget(bulk_edit_button);
    select_layout->addStretch(1);
    scroll_layout->addLayout(select_layout);

    show_subcategories_checkbox = new QCheckBox(this);
    show_subcategories_checkbox->setChecked(show_subcategory_column);
    scroll_layout->addWidget(show_subcategories_checkbox);

    dry_run_checkbox = new QCheckBox(this);
    dry_run_checkbox->setChecked(false);
    scroll_layout->addWidget(dry_run_checkbox);

    rename_images_only_checkbox = new QCheckBox(this);
    rename_images_only_checkbox->setChecked(false);
    rename_images_only_checkbox->setEnabled(false);
    scroll_layout->addWidget(rename_images_only_checkbox);

    rename_documents_only_checkbox = new QCheckBox(this);
    rename_documents_only_checkbox->setChecked(false);
    rename_documents_only_checkbox->setEnabled(false);
    scroll_layout->addWidget(rename_documents_only_checkbox);

    model = new QStandardItemModel(this);
    model->setColumnCount(8);

    table_view = new QTableView(this);
    table_view->setObjectName(QStringLiteral("aifsReviewTable"));
    table_view->setModel(model);
    table_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_view->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    table_view->horizontalHeader()->setStretchLastSection(true);
    table_view->verticalHeader()->setVisible(false);
    table_view->horizontalHeader()->setSectionsClickable(true);
    table_view->horizontalHeader()->setSortIndicatorShown(true);
    table_view->setSortingEnabled(true);
    table_view->setAlternatingRowColors(true);
    table_view->setFrameShape(QFrame::NoFrame);
    table_view->setColumnHidden(ColumnType, false);
    table_view->setColumnHidden(ColumnSuggestedName, !show_rename_column);
    table_view->setColumnHidden(ColumnSubcategory, !show_subcategory_column);
    table_view->setColumnHidden(ColumnPreview, false);
    table_view->setColumnWidth(ColumnSelect, 70);
    table_view->setIconSize(QSize(16, 16));
    table_view->setColumnWidth(ColumnType, table_view->iconSize().width() + 12);

    auto* table_panel = new QFrame(this);
    table_panel->setObjectName(QStringLiteral("aifsReviewTablePanel"));
    auto* table_panel_layout = new QVBoxLayout(table_panel);
    table_panel_layout->setContentsMargins(2, 2, 2, 2);
    table_panel_layout->setSpacing(0);
    table_panel_layout->addWidget(table_view);
    scroll_layout->addWidget(table_panel, 1);

    auto* tip_label = new QLabel(this);
    tip_label->setWordWrap(true);
    QFont tip_font = tip_label->font();
    tip_font.setItalic(true);
    tip_label->setFont(tip_font);
    tip_label->setTextFormat(Qt::RichText);
    tip_label->setText(tr("Tip: Double-click %1 cells to edit them.")
                           .arg(edit_icon_html()));
    scroll_layout->addWidget(tip_label);

    confirm_button = new QPushButton(this);
    continue_button = new QPushButton(this);
    undo_button = new QPushButton(this);
    undo_button->setEnabled(false);
    undo_button->setVisible(false);
    close_button = new QPushButton(this);
    close_button->setVisible(false);
    preview_button = new QPushButton(this);
    preview_button->setEnabled(false);

    for (QPushButton* button : {preview_button,
                                confirm_button,
                                continue_button,
                                undo_button,
                                close_button}) {
        if (button) {
            button->setProperty("aifsReviewActionButton", true);
            button->setCursor(Qt::PointingHandCursor);
        }
    }
    if (confirm_button) {
        confirm_button->setObjectName(QStringLiteral("aifsReviewPrimaryButton"));
    }

    scroll_area->setWidget(scroll_widget);
    layout->addWidget(scroll_area, 1);

    auto* bottom_layout = new QHBoxLayout();
    bottom_layout->setContentsMargins(0, 0, 0, 0);
    bottom_layout->setSpacing(8);
    bottom_layout->addStretch(1);
    bottom_layout->addWidget(preview_button);
    bottom_layout->addWidget(confirm_button);
    bottom_layout->addWidget(continue_button);
    bottom_layout->addWidget(undo_button);
    bottom_layout->addWidget(close_button);

    layout->addLayout(bottom_layout);

    if (auto* screen = this->screen()) {
        const QRect available = screen->availableGeometry();
        const int max_height = static_cast<int>(available.height() * 0.9);
        setMaximumHeight(max_height);
    }

    connect(confirm_button, &QPushButton::clicked, this, &CategorizationDialog::on_confirm_and_sort_button_clicked);
    connect(continue_button, &QPushButton::clicked, this, &CategorizationDialog::on_continue_later_button_clicked);
    connect(close_button, &QPushButton::clicked, this, &CategorizationDialog::accept);
    connect(undo_button, &QPushButton::clicked, this, &CategorizationDialog::on_undo_button_clicked);
    connect(preview_button, &QPushButton::clicked, this, &CategorizationDialog::on_preview_button_clicked);
    connect(select_all_checkbox, &QCheckBox::toggled, this, &CategorizationDialog::on_select_all_toggled);
    connect(auto_approve_filenames_checkbox, &QCheckBox::toggled,
            this, &CategorizationDialog::on_auto_approve_filename_changes_toggled);
    connect(auto_approve_categories_checkbox, &QCheckBox::toggled,
            this, &CategorizationDialog::on_auto_approve_categorization_toggled);
    connect(select_highlighted_button, &QPushButton::clicked, this, &CategorizationDialog::on_select_highlighted_clicked);
    connect(bulk_edit_button, &QPushButton::clicked, this, &CategorizationDialog::on_bulk_edit_clicked);
    connect(model, &QStandardItemModel::itemChanged, this, &CategorizationDialog::on_item_changed);
    connect(table_view, &QTableView::doubleClicked, this, &CategorizationDialog::on_table_double_clicked);
    connect(show_subcategories_checkbox, &QCheckBox::toggled,
            this, &CategorizationDialog::on_show_subcategories_toggled);
    connect(rename_images_only_checkbox, &QCheckBox::toggled,
            this, &CategorizationDialog::on_rename_images_only_toggled);
    connect(rename_documents_only_checkbox, &QCheckBox::toggled,
            this, &CategorizationDialog::on_rename_documents_only_toggled);

    auto* select_highlighted_shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Space), this);
    connect(select_highlighted_shortcut, &QShortcut::activated,
            this, &CategorizationDialog::on_select_highlighted_clicked);

    if (table_view && table_view->selectionModel()) {
        connect(table_view->selectionModel(),
                &QItemSelectionModel::selectionChanged,
                this,
                [this]() { update_preview_button_state(); });
    }
}

void CategorizationDialog::apply_theme_styles()
{
    if (applying_theme_styles_) {
        return;
    }

    QScopedValueRollback<bool> rollback(applying_theme_styles_, true);
    const QString style_sheet = AppTheme::review_dialog_style_sheet(QApplication::palette());
    if (styleSheet() != style_sheet) {
        setStyleSheet(style_sheet);
    }
}


namespace {
QFileIconProvider& file_icon_provider()
{
    static QFileIconProvider provider;
    return provider;
}

QIcon fallback_image_icon()
{
    static QIcon icon;
    if (!icon.isNull()) {
        return icon;
    }

    auto make_pixmap = [](int size) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPen frame_pen(QColor(120, 120, 120));
        frame_pen.setWidthF(1.0);
        painter.setPen(frame_pen);
        painter.setBrush(QColor(240, 240, 240));
        painter.drawRoundedRect(QRectF(1, 1, size - 2, size - 2), 2, 2);

        QRectF image_rect(3, 4, size - 6, size - 7);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(140, 200, 245));
        painter.drawRect(image_rect);

        QPolygonF mountain;
        mountain << QPointF(image_rect.left() + 1, image_rect.bottom() - 1)
                 << QPointF(image_rect.center().x() - 1, image_rect.top() + 2)
                 << QPointF(image_rect.right() - 1, image_rect.bottom() - 1);
        painter.setBrush(QColor(90, 170, 125));
        painter.drawPolygon(mountain);

        painter.setBrush(QColor(255, 210, 80));
        painter.drawEllipse(QPointF(image_rect.right() - 3, image_rect.top() + 3), 1.6, 1.6);

        return pixmap;
    };

    icon.addPixmap(make_pixmap(16));
    icon.addPixmap(make_pixmap(32));
    return icon;
}

#if defined(Q_OS_WIN)
bool icons_match(const QIcon& left, const QIcon& right, const QSize& size)
{
    if (left.isNull() || right.isNull()) {
        return false;
    }
    const QPixmap left_pixmap = left.pixmap(size);
    const QPixmap right_pixmap = right.pixmap(size);
    if (left_pixmap.isNull() || right_pixmap.isNull()) {
        return false;
    }
    const QImage left_image = left_pixmap.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const QImage right_image = right_pixmap.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (left_image.size() != right_image.size()) {
        return false;
    }
    return left_image == right_image;
}
#endif

QIcon type_icon(const QString& code, const QString& file_path)
{
    if (auto* style = QApplication::style()) {
        if (code == QStringLiteral("D")) {
            return style->standardIcon(QStyle::SP_DirIcon);
        }
        if (code == QStringLiteral("I")) {
            QIcon icon = QIcon::fromTheme(QStringLiteral("image-x-generic"));
            if (icon.isNull()) {
                icon = QIcon::fromTheme(QStringLiteral("image"));
            }
            if (icon.isNull()) {
                icon = QIcon::fromTheme(QStringLiteral("image-x-generic-symbolic"));
            }
            if (icon.isNull() && !file_path.isEmpty()) {
                icon = file_icon_provider().icon(QFileInfo(file_path));
            }
            if (icon.isNull()) {
                icon = fallback_image_icon();
            }
#if defined(Q_OS_WIN)
            if (icons_match(icon, style->standardIcon(QStyle::SP_FileIcon), QSize(16, 16))) {
                icon = fallback_image_icon();
            }
#endif
            return icon.isNull() ? style->standardIcon(QStyle::SP_FileIcon) : icon;
        }
        return style->standardIcon(QStyle::SP_FileIcon);
    }
    return {};
}

QIcon edit_icon()
{
    QIcon icon = QIcon::fromTheme(QStringLiteral("edit-rename"));
    if (!icon.isNull()) {
        return icon;
    }
    icon = QIcon::fromTheme(QStringLiteral("document-edit"));
    if (!icon.isNull()) {
        return icon;
    }
    if (auto* style = QApplication::style()) {
        return style->standardIcon(QStyle::SP_FileDialogDetailedView);
    }
    return QIcon();
}

QString edit_icon_html(int size)
{
    const QIcon icon = edit_icon();
    const QPixmap pixmap = icon.pixmap(size, size);
    if (pixmap.isNull()) {
        return QStringLiteral("[edit]");
    }
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    pixmap.save(&buffer, "PNG");
    return QStringLiteral("<img src=\"data:image/png;base64,%1\" width=\"%2\" height=\"%2\"/>")
        .arg(QString::fromLatin1(bytes.toBase64()))
        .arg(size);
}

}

void CategorizationDialog::ensure_unique_suggested_names_in_model()
{
    if (!model) {
        return;
    }

    std::vector<CategorizedFile> entries;
    std::vector<int> rows;
    entries.reserve(model->rowCount());
    rows.reserve(model->rowCount());

    for (int row = 0; row < model->rowCount(); ++row) {
        auto* file_item = model->item(row, ColumnFile);
        auto* rename_item = model->item(row, ColumnSuggestedName);
        if (!file_item || !rename_item) {
            continue;
        }
        const std::string suggested = rename_item->text().toStdString();
        if (suggested.empty()) {
            continue;
        }

        CategorizedFile entry;
        entry.file_path = file_item->data(kFilePathRole).toString().toStdString();
        if (entry.file_path.empty()) {
            entry.file_path = base_dir_;
        }
        entry.file_name = file_item->text().toStdString();
        entry.type = static_cast<FileType>(file_item->data(kFileTypeRole).toInt());
        if (entry.type != FileType::File) {
            continue;
        }
        entry.rename_only = file_item->data(kRenameOnlyRole).toBool();
        entry.rename_applied = file_item->data(kRenameAppliedRole).toBool();
        if (auto* category_item = model->item(row, ColumnCategory)) {
            entry.category = category_item->text().toStdString();
            if (is_missing_category_label(entry.category)) {
                entry.category.clear();
            }
        }
        if (auto* subcategory_item = model->item(row, ColumnSubcategory)) {
            entry.subcategory = subcategory_item->text().toStdString();
            if (is_missing_category_label(entry.subcategory)) {
                entry.subcategory.clear();
            }
        }
        entry.suggested_name = suggested;
        entries.push_back(std::move(entry));
        rows.push_back(row);
    }

    ensure_unique_suggested_names(entries, base_dir_, show_subcategory_column);

    for (std::size_t index = 0; index < entries.size(); ++index) {
        const auto row = rows.at(index);
        if (auto* rename_item = model->item(row, ColumnSuggestedName)) {
            if (rename_item->text().toStdString() != entries.at(index).suggested_name) {
                rename_item->setText(QString::fromStdString(entries.at(index).suggested_name));
            }
        }
    }
}

void CategorizationDialog::populate_model()
{
    ScopedFlag guard(suppress_item_changed_);
    model->removeRows(0, model->rowCount());

    const int type_col_width = table_view ? table_view->iconSize().width() + 12 : 28;
    if (table_view) {
        table_view->setColumnWidth(2, type_col_width);
    }

    updating_select_all = true;

    for (const auto& file : categorized_files) {
        QList<QStandardItem*> row;

        auto* select_item = new QStandardItem;
        select_item->setCheckable(true);
        select_item->setCheckState(Qt::Checked);
        select_item->setEditable(false);
        select_item->setData(Qt::AlignCenter, Qt::TextAlignmentRole);

        auto* file_item = new QStandardItem(QString::fromStdString(file.file_name));
        file_item->setEditable(false);
        file_item->setData(QString::fromStdString(file.file_path), kFilePathRole);
        file_item->setData(QString::fromStdString(file.file_name), kOriginalFileNameRole);
        file_item->setData(file.used_consistency_hints, kUsedConsistencyRole);
        file_item->setData(file.rename_only, kRenameOnlyRole);
        file_item->setData(static_cast<int>(file.type), kFileTypeRole);
        file_item->setData(QString::fromStdString(file.learning_context), kLearningContextRole);
        const bool rename_locked = file.rename_applied ||
                                   (!file.suggested_name.empty() &&
                                    to_lower_copy_str(file.suggested_name) == to_lower_copy_str(file.file_name));
        file_item->setData(file.rename_applied, kRenameAppliedRole);
        file_item->setData(rename_locked, kRenameLockedRole);

        const bool is_image_entry = is_supported_image_entry(file.file_path, file.file_name, file.type);

        auto* type_item = new QStandardItem;
        type_item->setEditable(false);
        if (file.type == FileType::Directory) {
            type_item->setData(QStringLiteral("D"), Qt::UserRole);
        } else if (is_image_entry) {
            type_item->setData(QStringLiteral("I"), Qt::UserRole);
        } else {
            type_item->setData(QStringLiteral("F"), Qt::UserRole);
        }
        type_item->setData(Qt::AlignCenter, Qt::TextAlignmentRole);
        update_type_icon(type_item);

        const std::string suggested_name = rename_locked ? std::string() : file.suggested_name;
        auto* suggested_item = new QStandardItem(QString::fromStdString(suggested_name));
        const bool allow_rename = !suggested_name.empty();
        suggested_item->setEditable(allow_rename);
        if (allow_rename) {
            suggested_item->setIcon(edit_icon());
        }

        std::string category_text = file.category;
        if (is_image_entry && is_missing_category_label(category_text)) {
            category_text.clear();
        }
        auto* category_item = new QStandardItem(QString::fromStdString(category_text));
        category_item->setData(QString::fromStdString(category_text), kOriginalCategoryRole);
        category_item->setData(QString::fromStdString(
            file.canonical_category.empty() ? file.category : file.canonical_category), kCanonicalCategoryRole);
        category_item->setEditable(!file.rename_only);
        if (!file.rename_only) {
            category_item->setIcon(edit_icon());
        }

        std::string subcategory_text = file.subcategory;
        if (is_image_entry && is_missing_category_label(subcategory_text)) {
            subcategory_text.clear();
        }
        auto* subcategory_item = new QStandardItem(QString::fromStdString(subcategory_text));
        subcategory_item->setData(QString::fromStdString(subcategory_text), kOriginalSubcategoryRole);
        subcategory_item->setData(QString::fromStdString(
            file.canonical_subcategory.empty() ? file.subcategory : file.canonical_subcategory), kCanonicalSubcategoryRole);
        subcategory_item->setEditable(!file.rename_only);
        if (!file.rename_only) {
            subcategory_item->setIcon(edit_icon());
        }

        auto* status_item = new QStandardItem;
        status_item->setEditable(false);
        status_item->setData(static_cast<int>(RowStatus::None), kStatusRole);
        apply_status_text(status_item);
        status_item->setForeground(QBrush());

        auto* preview_item = new QStandardItem;
        preview_item->setEditable(false);

        row << select_item
            << file_item
            << type_item
            << suggested_item
            << category_item
            << subcategory_item
            << status_item
            << preview_item;
        model->appendRow(row);
        update_preview_column(model->rowCount() - 1);
    }

    updating_select_all = false;
    apply_subcategory_visibility();
    apply_rename_visibility();
    table_view->resizeColumnsToContents();
    update_select_all_state();
    update_preview_button_state();
}

void CategorizationDialog::update_type_icon(QStandardItem* item)
{
    if (!item) {
        return;
    }

    const QString code = item->data(Qt::UserRole).toString();
    QString full_path;
    if (code == QStringLiteral("I") && model) {
        if (auto* file_item = model->item(item->row(), ColumnFile)) {
            const QString file_name = file_item->text();
            const QString base_dir = file_item->data(kFilePathRole).toString();
            if (!base_dir.isEmpty()) {
                full_path = QDir(base_dir).filePath(file_name);
            } else if (!base_dir_.empty()) {
                full_path = QDir(QString::fromStdString(base_dir_)).filePath(file_name);
            } else {
                full_path = file_name;
            }
        }
    }
    item->setIcon(type_icon(code, full_path));
    item->setText(QString());
}


void CategorizationDialog::record_categorization_to_db(bool learn_approved_mappings)
{
    if (!db_manager) {
        return;
    }

    auto entry_is_unchanged = [](const CategorizedFile& cached,
                                 const DatabaseManager::ResolvedCategory& resolved,
                                 const std::string& suggested_name,
                                 bool rename_only,
                                 bool used_consistency) {
        if (cached.rename_only != rename_only) {
            return false;
        }
        if (cached.suggested_name != suggested_name) {
            return false;
        }
        if (cached.used_consistency_hints != used_consistency) {
            return false;
        }
        if (cached.category != resolved.category || cached.subcategory != resolved.subcategory) {
            return false;
        }
        if (!resolved.category.empty() && cached.taxonomy_id != resolved.taxonomy_id) {
            return false;
        }
        return true;
    };
    auto read_role_text = [](QStandardItem* item, int role) {
        return item && item->data(role).isValid()
            ? item->data(role).toString().toStdString()
            : std::string();
    };
    auto update_category_roles = [](QStandardItem* item,
                                    const std::string& display_value,
                                    const std::string& canonical_value,
                                    int original_role,
                                    int canonical_role) {
        if (!item) {
            return;
        }
        item->setText(QString::fromStdString(display_value));
        item->setData(QString::fromStdString(display_value), original_role);
        item->setData(QString::fromStdString(canonical_value), canonical_role);
    };
    auto display_uses_canonical_storage = [&read_role_text](QStandardItem* category_item,
                                                            QStandardItem* subcategory_item,
                                                            const std::string& category,
                                                            const std::string& subcategory) {
        const std::string original_category = read_role_text(category_item, kOriginalCategoryRole);
        const std::string original_subcategory = read_role_text(subcategory_item, kOriginalSubcategoryRole);
        const std::string canonical_category = read_role_text(category_item, kCanonicalCategoryRole);
        return category == original_category &&
               subcategory == original_subcategory &&
               !canonical_category.empty();
    };
    auto resolve_for_storage = [this, &read_role_text, &display_uses_canonical_storage](
                                   QStandardItem* category_item,
                                   QStandardItem* subcategory_item,
                                   const std::string& category,
                                   const std::string& subcategory) {
        if (display_uses_canonical_storage(category_item, subcategory_item, category, subcategory)) {
            const std::string canonical_category = read_role_text(category_item, kCanonicalCategoryRole);
            const std::string canonical_subcategory = read_role_text(subcategory_item, kCanonicalSubcategoryRole);
            return db_manager->resolve_category(canonical_category, canonical_subcategory);
        }
        return db_manager->resolve_category_for_language(category,
                                                        subcategory,
                                                        category_language_);
    };

    for (int row = 0; row < model->rowCount(); ++row) {
        auto* file_item = model->item(row, ColumnFile);
        if (!file_item) {
            continue;
        }
        const auto* select_item = model->item(row, ColumnSelect);
        const bool selected_for_processing =
            !select_item || select_item->checkState() == Qt::Checked;
        bool rename_only = file_item->data(kRenameOnlyRole).toBool();
        auto* category_item = model->item(row, ColumnCategory);
        auto* subcategory_item = model->item(row, ColumnSubcategory);
        std::string category = read_item_or_hidden_text(category_item, kHiddenCategoryRole);
        std::string subcategory = read_item_or_hidden_text(subcategory_item, kHiddenSubcategoryRole);
        const bool is_image = row_is_supported_image(row);
        const bool is_document = row_is_supported_document(row);
        if (is_image || is_document) {
            if (is_missing_category_label(category)) {
                category.clear();
            }
            if (is_missing_category_label(subcategory)) {
                subcategory.clear();
            }
        }
        if (!rename_only && (is_image || is_document) && category.empty()) {
            rename_only = true;
        }
        const auto* suggested_item = model->item(row, ColumnSuggestedName);
        const std::string suggested_name = suggested_item
                                               ? suggested_item->text().toStdString()
                                               : std::string();

        const std::string file_name = file_item->text().toStdString();
        const std::string file_path = file_item->data(kFilePathRole).toString().toStdString();
        const bool used_consistency = file_item->data(kUsedConsistencyRole).toBool();
        const FileType file_type = static_cast<FileType>(file_item->data(kFileTypeRole).toInt());
        const auto cached_entry = db_manager->get_categorized_file(file_path, file_name, file_type);
        if (rename_only) {
            if (cached_entry) {
                const auto localized_cached = db_manager->localize_categorized_file(*cached_entry, category_language_);
                category = localized_cached.category;
                subcategory = localized_cached.subcategory;
            } else {
                category.clear();
                subcategory.clear();
            }
            if (is_missing_category_label(category)) {
                category.clear();
            }
            if (is_missing_category_label(subcategory)) {
                subcategory.clear();
            }
            if (category_item) {
                category_item->setText(QString::fromStdString(category));
            }
            if (show_subcategory_column && subcategory_item) {
                subcategory_item->setText(QString::fromStdString(subcategory));
            }
            if (cached_entry) {
                update_category_roles(category_item,
                                      category,
                                      cached_entry->category,
                                      kOriginalCategoryRole,
                                      kCanonicalCategoryRole);
                update_category_roles(subcategory_item,
                                      subcategory,
                                      cached_entry->subcategory,
                                      kOriginalSubcategoryRole,
                                      kCanonicalSubcategoryRole);
            }
        }
        if (rename_only) {
            DatabaseManager::ResolvedCategory resolved{0, "", ""};
            if (cached_entry && !cached_entry->category.empty()) {
                resolved.taxonomy_id = cached_entry->taxonomy_id;
                resolved.category = cached_entry->category;
                resolved.subcategory = cached_entry->subcategory;
            }
            const std::string file_type_label = (file_type == FileType::Directory) ? "D" : "F";
            if (cached_entry &&
                entry_is_unchanged(*cached_entry, resolved, suggested_name, rename_only, used_consistency)) {
                continue;
            }
            db_manager->insert_or_update_file_with_categorization(
                file_name, file_type_label, file_path, resolved, used_consistency, suggested_name, true);
            continue;
        }

        if (!category_item) {
            continue;
        }

        const bool preserve_display_labels =
            display_uses_canonical_storage(category_item, subcategory_item, category, subcategory);
        auto resolved = resolve_for_storage(category_item, subcategory_item, category, subcategory);
        if (learn_approved_mappings && selected_for_processing && learning_store_ &&
            !resolved.category.empty()) {
            std::string learning_error;
            UserLearningStore::ApprovedMapping mapping;
            mapping.file_name = file_name;
            mapping.file_type = file_type;
            mapping.dir_path = file_path;
            mapping.category = resolved.category;
            mapping.subcategory = resolved.subcategory;
            mapping.suggested_name = suggested_name;
            mapping.context_text = file_item->data(kLearningContextRole).toString().toStdString();
            mapping.used_consistency_hints = used_consistency;
            if (!learning_store_->record_approved_mapping(mapping, &learning_error) && db_logger) {
                db_logger->warn("Failed to record learned categorization for '{}': {}",
                                file_name,
                                learning_error);
            }
        }

        const std::string file_type_label = (file_type == FileType::Directory) ? "D" : "F";
        if (cached_entry &&
            entry_is_unchanged(*cached_entry, resolved, suggested_name, rename_only, used_consistency)) {
            continue;
        }
        db_manager->insert_or_update_file_with_categorization(
            file_name, file_type_label, file_path, resolved, used_consistency, suggested_name);

        if (preserve_display_labels) {
            update_category_roles(category_item,
                                  category,
                                  resolved.category,
                                  kOriginalCategoryRole,
                                  kCanonicalCategoryRole);
            if (show_subcategory_column) {
                update_category_roles(subcategory_item,
                                      subcategory,
                                      resolved.subcategory,
                                      kOriginalSubcategoryRole,
                                      kCanonicalSubcategoryRole);
            }
        } else {
            const auto display_resolved = db_manager->localize_category(resolved, category_language_);
            update_category_roles(category_item,
                                  display_resolved.category,
                                  resolved.category,
                                  kOriginalCategoryRole,
                                  kCanonicalCategoryRole);
            if (show_subcategory_column) {
                update_category_roles(subcategory_item,
                                      display_resolved.subcategory,
                                      resolved.subcategory,
                                      kOriginalSubcategoryRole,
                                      kCanonicalSubcategoryRole);
            }
        }
    }
}


void CategorizationDialog::on_confirm_and_sort_button_clicked()
{
    const bool dry_run = dry_run_checkbox && dry_run_checkbox->isChecked();
    record_categorization_to_db(!dry_run);

    if (categorized_files.empty()) {
        if (ui_logger) {
            ui_logger->warn("No categorized files available for sorting.");
        }
        return;
    }

    const std::string base_dir = base_dir_.empty()
        ? categorized_files.front().file_path
        : base_dir_;
    dry_run_plan_.clear();
    clear_move_history();
    if (undo_button) {
        undo_button->setEnabled(false);
        undo_button->setVisible(false);
    }

    if (dry_run && core_logger) {
        core_logger->info("Dry run enabled; will not move files.");
    }

    std::vector<std::string> files_not_moved;
    ScopedFlag guard(suppress_item_changed_);
    if (include_subdirectories_) {
        struct CollisionState {
            std::unordered_set<std::string> used_names;
            std::unordered_map<std::string, int> next_index;
        };
        std::unordered_map<std::string, CollisionState> collisions;
        collisions.reserve(static_cast<size_t>(model->rowCount()));

        for (int row_index = 0; row_index < model->rowCount(); ++row_index) {
            auto* select_item = model->item(row_index, ColumnSelect);
            if (select_item && select_item->checkState() != Qt::Checked) {
                continue;
            }
            auto* file_item = model->item(row_index, ColumnFile);
            auto* category_item = model->item(row_index, ColumnCategory);
            auto* subcategory_item = model->item(row_index, ColumnSubcategory);
            auto* rename_item = model->item(row_index, ColumnSuggestedName);
            if (!file_item || !category_item) {
                continue;
            }

            bool rename_only = false;
            bool used_consistency_hints = false;
            FileType file_type = FileType::File;
            if (!resolve_row_flags(row_index, rename_only, used_consistency_hints, file_type)) {
                continue;
            }
            (void)used_consistency_hints;
            if (rename_only) {
                continue;
            }

            std::string category = read_item_or_hidden_text(category_item, kHiddenCategoryRole);
            if (is_missing_category_label(category)) {
                continue;
            }
            std::string subcategory = read_item_or_hidden_text(subcategory_item, kHiddenSubcategoryRole);
            if (is_missing_category_label(subcategory)) {
                subcategory.clear();
            }

            const std::string file_name = file_item->text().toStdString();
            const std::string rename_candidate = rename_item
                                                     ? rename_item->text().toStdString()
                                                     : std::string();
            std::string source_dir = file_item->data(kFilePathRole).toString().toStdString();
            if (source_dir.empty()) {
                source_dir = base_dir;
            }

            CategorizedFile file;
            file.file_path = source_dir;
            file.file_name = file_name;
            file.type = file_type;
            file.category = category;
            file.subcategory = subcategory;
            file.rename_only = false;

            const auto target_dir = build_suggested_target_dir(file, base_dir, show_subcategory_column);
            const std::string dir_key = Utils::path_to_utf8(target_dir);
            auto& state = collisions[dir_key];
            const std::string desired_name = resolve_destination_name(file_name, rename_candidate);
            const std::string unique_name = build_unique_move_name(desired_name,
                                                                   target_dir,
                                                                   state.used_names,
                                                                   state.next_index);
            state.used_names.insert(to_lower_copy_str(unique_name));
            if (unique_name != desired_name) {
                if (rename_item) {
                    rename_item->setText(QString::fromStdString(unique_name));
                }
                update_preview_column(row_index);
            }
        }
    }

    for (int row_index = 0; row_index < model->rowCount(); ++row_index) {
        auto* select_item = model->item(row_index, ColumnSelect);
        if (select_item && select_item->checkState() != Qt::Checked) {
            update_status_column(row_index, false, false);
            continue;
        }

        auto* file_item = model->item(row_index, ColumnFile);
        auto* category_item = model->item(row_index, ColumnCategory);
        auto* subcategory_item = model->item(row_index, ColumnSubcategory);
        auto* rename_item = model->item(row_index, ColumnSuggestedName);
        if (!file_item || !category_item) {
            update_status_column(row_index, false);
            continue;
        }

        bool rename_only = false;
        bool used_consistency_hints = false;
        FileType file_type = FileType::File;
        if (!resolve_row_flags(row_index, rename_only, used_consistency_hints, file_type)) {
            update_status_column(row_index, false);
            continue;
        }

        const std::string file_name = file_item->text().toStdString();
        const std::string category = read_item_or_hidden_text(category_item, kHiddenCategoryRole);
        const std::string subcategory = read_item_or_hidden_text(subcategory_item, kHiddenSubcategoryRole);
        const std::string rename_candidate = rename_item
                                                 ? rename_item->text().toStdString()
                                                 : std::string();
        std::string source_dir = file_item->data(kFilePathRole).toString().toStdString();
        if (source_dir.empty()) {
            source_dir = base_dir;
        }

        handle_selected_row(row_index,
                            file_name,
                            rename_candidate,
                            category,
                            subcategory,
                            source_dir,
                            base_dir,
                            files_not_moved,
                            file_type,
                            rename_only,
                            used_consistency_hints,
                            dry_run);
    }

    if (dry_run) {
        if (core_logger) {
            core_logger->info("Dry run completed. No files were moved.");
        }
    } else if (files_not_moved.empty()) {
        if (core_logger) {
            core_logger->info("All files have been sorted and moved successfully.");
        }
    } else if (ui_logger) {
        ui_logger->info("Categorization complete. Unmoved files: {}", files_not_moved.size());
    }

    if (dry_run) {
        // Show preview dialog of planned moves.
        std::vector<DryRunPreviewDialog::Entry> entries;
        entries.reserve(static_cast<size_t>(model->rowCount()));
        for (int row = 0; row < model->rowCount(); ++row) {
            if (auto* select_item = model->item(row, ColumnSelect)) {
                if (select_item->checkState() != Qt::Checked) {
                    continue;
                }
            }
            const auto* file_item = model->item(row, ColumnFile);
            const auto* cat_item = model->item(row, ColumnCategory);
            if (!file_item || !cat_item) {
                continue;
            }
            std::string debug_reason;
            auto rec = build_preview_record_for_row(row, &debug_reason);
            if (!rec) {
                if (core_logger) {
                    core_logger->warn("Dry run preview skipped row {}: {}", row, debug_reason);
                }
                continue;
            }
            const bool rename_images_only_active = rename_images_only_checkbox &&
                                                   rename_images_only_checkbox->isChecked() &&
                                                   row_is_supported_image(row);
            std::string to_label;
            std::string destination;
#ifdef _WIN32
            const char sep = '\\\\';
#else
            const char sep = '/';
#endif
            if (rename_images_only_active) {
                to_label = rec->destination_file_name;
                if (!base_dir.empty()) {
                    destination = Utils::path_to_utf8(
                        Utils::utf8_to_path(base_dir) / Utils::utf8_to_path(rec->destination_file_name));
                } else {
                    destination = rec->destination;
                }
            } else if (rec->rename_only) {
                to_label = rec->destination_file_name;
                destination = rec->destination;
            } else {
                to_label = rec->category;
                if (rec->use_subcategory && !rec->subcategory.empty()) {
                    to_label += std::string(1, sep) + rec->subcategory;
                }
                to_label += std::string(1, sep) + rec->destination_file_name;
                destination = rec->destination;
            }
#ifdef _WIN32
            std::replace(destination.begin(), destination.end(), '/', '\\');
#endif
            std::string source_tooltip = rec->source;
#ifdef _WIN32
            std::replace(source_tooltip.begin(), source_tooltip.end(), '/', '\\');
#endif

            entries.push_back(DryRunPreviewDialog::Entry{
                /*from_label*/ rec->source_file_name,
                /*to_label*/ to_label,
                /*source_tooltip*/ source_tooltip,
                /*destination_tooltip*/ destination});
        }
        if (core_logger) {
            core_logger->info("Dry run preview entries built: {}", entries.size());
        }
        DryRunPreviewDialog preview_dialog(entries, this);
        preview_dialog.exec();

        // In preview mode, keep the dialog actionable so the user can uncheck Dry run and re-run.
        if (undo_button) {
            undo_button->setVisible(false);
            undo_button->setEnabled(false);
        }
        restore_action_buttons();
        return;
    }

    if (!move_history_.empty() && undo_button) {
        undo_button->setVisible(true);
        undo_button->setEnabled(true);
    }

    if (!move_history_.empty()) {
        persist_move_plan();
    }

    show_close_button();
}

void CategorizationDialog::handle_selected_row(int row_index,
                                               const std::string& file_name,
                                               const std::string& rename_candidate,
                                               const std::string& category,
                                               const std::string& subcategory,
                                               const std::string& source_dir,
                                               const std::string& base_dir,
                                               std::vector<std::string>& files_not_moved,
                                               FileType file_type,
                                               bool rename_only,
                                               bool used_consistency_hints,
                                               bool dry_run)
{
    const std::string destination_name = resolve_destination_name(file_name, rename_candidate);
    const bool rename_active = destination_name != file_name;
    auto* category_item_ref = model ? model->item(row_index, ColumnCategory) : nullptr;
    auto* subcategory_item_ref = model ? model->item(row_index, ColumnSubcategory) : nullptr;
    auto read_role_text = [](QStandardItem* item, int role) {
        return item && item->data(role).isValid()
            ? item->data(role).toString().toStdString()
            : std::string();
    };
    auto apply_successful_rename = [this, row_index, &destination_name]() {
        if (!model) {
            return;
        }
        if (auto* file_item = model->item(row_index, ColumnFile)) {
            if (!file_item->data(kOriginalFileNameRole).isValid()) {
                file_item->setData(file_item->text(), kOriginalFileNameRole);
            }
            file_item->setData(true, kRenameAppliedRole);
            file_item->setData(true, kRenameLockedRole);
        }
        if (auto* rename_item = model->item(row_index, ColumnSuggestedName)) {
            rename_item->setText(QString::fromStdString(destination_name));
        }
        update_preview_column(row_index);
    };

    if (auto& probe = move_probe_slot()) {
        const std::string effective_subcategory = subcategory.empty() ? category : subcategory;
        probe(TestHooks::CategorizationMoveInfo{
            show_subcategory_column,
            category,
            effective_subcategory,
            file_name
        });
        update_status_column(row_index, true, true, rename_active, !rename_only);
        return;
    }

    if (rename_active) {
        std::string rename_error;
        if (!validate_filename(destination_name, rename_error)) {
            update_status_column(row_index, false);
            files_not_moved.push_back(file_name);
            if (core_logger) {
                core_logger->warn("Skipping rename for '{}' due to invalid filename: {} (rename='{}')",
                                  file_name,
                                  rename_error,
                                  destination_name);
            }
            return;
        }
    }

    if (rename_only) {
        if (!rename_active) {
            update_status_column(row_index, true, true, false, false);
            return;
        }

        const auto source_path = Utils::utf8_to_path(source_dir) / Utils::utf8_to_path(file_name);
        const auto dest_path = Utils::utf8_to_path(source_dir) / Utils::utf8_to_path(destination_name);

        if (dry_run) {
            const std::string source_display = Utils::path_to_utf8(source_path);
            const std::string dest_display = Utils::path_to_utf8(dest_path);
            set_preview_status(row_index, dest_display);
            dry_run_plan_.push_back(PreviewRecord{
                source_display,
                dest_display,
                file_name,
                destination_name,
                std::string(),
                std::string(),
                false,
                true});
            if (core_logger) {
                core_logger->info("Dry run: would rename '{}' to '{}'",
                                  source_display,
                                  dest_display);
            }
            return;
        }

        if (!storage_provider_ || !storage_provider_->path_exists(Utils::path_to_utf8(source_path))) {
            update_status_column(row_index, false);
            files_not_moved.push_back(file_name);
            if (core_logger) {
                core_logger->warn("Source file missing when renaming '{}'", file_name);
            }
            return;
        }
        if (!storage_provider_ || storage_provider_->path_exists(Utils::path_to_utf8(dest_path))) {
            update_status_column(row_index, false);
            files_not_moved.push_back(file_name);
            if (core_logger) {
                core_logger->warn("Destination already exists for rename '{}'", destination_name);
            }
            return;
        }

        const auto move_result = storage_provider_->move_entry(Utils::path_to_utf8(source_path),
                                                               Utils::path_to_utf8(dest_path));
        if (move_result.success) {
            update_status_column(row_index, true, true, rename_active, false);
            const std::string source_text = Utils::path_to_utf8(source_path);
            const std::string destination_text = Utils::path_to_utf8(dest_path);
            const long long history_id = record_review_history(row_index,
                                                               "rename",
                                                               source_text,
                                                               destination_text,
                                                               file_name,
                                                               destination_name,
                                                               std::string(),
                                                               std::string(),
                                                               move_result.metadata.size_bytes,
                                                               move_result.metadata.mtime,
                                                               move_result.metadata.stable_identity,
                                                               move_result.metadata.revision_token);
            record_move_for_undo(row_index,
                                 source_text,
                                 destination_text,
                                 move_result.metadata.size_bytes,
                                 move_result.metadata.mtime,
                                 move_result.metadata.stable_identity,
                                 move_result.metadata.revision_token,
                                 history_id);
            if (db_manager) {
                DatabaseManager::ResolvedCategory resolved{0, "", ""};
                if (auto cached = db_manager->get_categorized_file(source_dir, file_name, file_type)) {
                    if (!is_missing_category_label(cached->category)) {
                        resolved.category = cached->category;
                        if (!is_missing_category_label(cached->subcategory)) {
                            resolved.subcategory = cached->subcategory;
                        }
                        resolved.taxonomy_id = cached->taxonomy_id;
                    }
                }
                const std::string file_type_label = (file_type == FileType::Directory) ? "D" : "F";
                db_manager->remove_file_categorization(source_dir, file_name, file_type);
                db_manager->insert_or_update_file_with_categorization(
                    destination_name,
                    file_type_label,
                    source_dir,
                    resolved,
                    used_consistency_hints,
                    destination_name,
                    true,
                    true);
            }
            apply_successful_rename();
        } else {
            update_status_column(row_index, false);
            files_not_moved.push_back(file_name);
            if (core_logger) {
                core_logger->error("Failed to rename '{}': {}",
                                   file_name,
                                   move_result.message.empty() ? "unknown error" : move_result.message);
            }
        }
        return;
    }

    const std::string effective_subcategory = subcategory.empty() ? category : subcategory;
    std::string validation_error;
    const bool allow_identical = !show_subcategory_column;
    if (!validate_labels(category, effective_subcategory, validation_error, allow_identical)) {
        update_status_column(row_index, false);
        files_not_moved.push_back(file_name);
        if (core_logger) {
            core_logger->warn("Skipping move for '{}' due to invalid category/subcategory: {} (cat='{}', sub='{}')",
                              file_name,
                              validation_error,
                              category,
                              effective_subcategory);
        }
        return;
    }

    try {
        MovableCategorizedFile categorized_file(
            *storage_provider_, source_dir, base_dir, category, effective_subcategory, file_name, destination_name);

        const auto preview_paths = categorized_file.preview_move_paths(show_subcategory_column);

        if (dry_run) {
            set_preview_status(row_index, preview_paths.destination);
            dry_run_plan_.push_back(PreviewRecord{
                preview_paths.source,
                preview_paths.destination,
                file_name,
                destination_name,
                category,
                effective_subcategory,
                show_subcategory_column,
                false});
            if (core_logger) {
                core_logger->info("Dry run: would move '{}' to '{}'",
                                  preview_paths.source,
                                  preview_paths.destination);
            }
            return;
        }

        categorized_file.create_cat_dirs(show_subcategory_column);
        const auto move_result = categorized_file.move_file(show_subcategory_column);
        update_status_column(row_index,
                             move_result.success,
                             true,
                             rename_active && move_result.success,
                             move_result.success);

        if (!move_result.success) {
            files_not_moved.push_back(file_name);
            if (core_logger) {
                core_logger->warn("File {} was not moved: {}",
                                  file_name,
                                  move_result.message.empty() ? "operation skipped" : move_result.message);
            }
        } else {
            const long long history_id = record_review_history(
                row_index,
                rename_active ? "rename_and_categorize" : "categorize",
                preview_paths.source,
                preview_paths.destination,
                file_name,
                destination_name,
                category,
                effective_subcategory,
                move_result.metadata.size_bytes,
                move_result.metadata.mtime,
                move_result.metadata.stable_identity,
                move_result.metadata.revision_token);
            record_move_for_undo(row_index,
                                 preview_paths.source,
                                 preview_paths.destination,
                                 move_result.metadata.size_bytes,
                                 move_result.metadata.mtime,
                                 move_result.metadata.stable_identity,
                                 move_result.metadata.revision_token,
                                 history_id);

            if (db_manager && (rename_active || include_subdirectories_)) {
                const std::string original_category = read_role_text(category_item_ref, kOriginalCategoryRole);
                const std::string original_subcategory = read_role_text(subcategory_item_ref, kOriginalSubcategoryRole);
                const std::string canonical_category = read_role_text(category_item_ref, kCanonicalCategoryRole);
                const std::string canonical_subcategory = read_role_text(subcategory_item_ref, kCanonicalSubcategoryRole);
                const std::string original_effective_subcategory =
                    original_subcategory.empty() ? original_category : original_subcategory;
                const bool unchanged_display =
                    category == original_category &&
                    effective_subcategory == original_effective_subcategory &&
                    !canonical_category.empty();
                auto resolved = unchanged_display
                    ? db_manager->resolve_category(canonical_category, canonical_subcategory)
                    : db_manager->resolve_category_for_language(category,
                                                                effective_subcategory,
                                                                category_language_);
                const std::string source_db_dir = include_subdirectories_ ? source_dir : base_dir;
                std::string destination_db_dir = base_dir;
                if (include_subdirectories_) {
                    const auto dest_parent = Utils::utf8_to_path(preview_paths.destination).parent_path();
                    destination_db_dir = Utils::path_to_utf8(dest_parent);
                }
                std::string suggested_name;
                bool rename_applied = rename_active;
                if (rename_active) {
                    suggested_name = destination_name;
                } else if (auto cached = db_manager->get_categorized_file(source_db_dir, file_name, file_type)) {
                    suggested_name = cached->suggested_name;
                    rename_applied = cached->rename_applied;
                }
                db_manager->remove_file_categorization(source_db_dir, file_name, file_type);
                db_manager->insert_or_update_file_with_categorization(
                    destination_name,
                    file_type == FileType::Directory ? "D" : "F",
                    destination_db_dir,
                    resolved,
                    used_consistency_hints,
                    suggested_name,
                    false,
                    rename_applied);
            }
            if (rename_active) {
                apply_successful_rename();
            }
        }
    } catch (const std::exception& ex) {
        update_status_column(row_index, false);
        files_not_moved.push_back(file_name);
        if (core_logger) {
            core_logger->error("Failed to move '{}': {}", file_name, ex.what());
        }
    }
}


void CategorizationDialog::on_continue_later_button_clicked()
{
    record_categorization_to_db();
    accept();
}

void CategorizationDialog::on_undo_button_clicked()
{
    if (!undo_move_history()) {
        return;
    }

    update_status_after_undo();
    restore_action_buttons();
    clear_move_history();
    if (undo_button) {
        undo_button->setEnabled(false);
        undo_button->setVisible(false);
    }
}


void CategorizationDialog::show_close_button()
{
    if (confirm_button) {
        confirm_button->setVisible(false);
    }
    if (continue_button) {
        continue_button->setVisible(false);
    }
    if (close_button) {
        close_button->setVisible(true);
    }
}

void CategorizationDialog::restore_action_buttons()
{
    if (confirm_button) {
        confirm_button->setVisible(true);
    }
    if (continue_button) {
        continue_button->setVisible(true);
    }
    if (close_button) {
        close_button->setVisible(false);
    }
}


void CategorizationDialog::update_status_column(int row,
                                                bool success,
                                                bool attempted,
                                                bool renamed,
                                                bool moved)
{
    if (auto* status_item = model->item(row, ColumnStatus)) {
        RowStatus status = RowStatus::None;
        const QPalette current_palette = palette();
        const QColor pending_color = current_palette.color(QPalette::Disabled, QPalette::Text);
        const QColor success_color = QColor(QStringLiteral("#2e9d57"));
        const QColor failure_color = QColor(QStringLiteral("#d64545"));
        if (!attempted) {
            status = RowStatus::NotSelected;
            status_item->setForeground(QBrush(pending_color));
        } else if (success) {
            if (renamed && moved) {
                status = RowStatus::RenamedAndMoved;
            } else if (renamed) {
                status = RowStatus::Renamed;
            } else {
                status = RowStatus::Moved;
            }
            status_item->setForeground(QBrush(success_color));
        } else {
            status = RowStatus::Skipped;
            status_item->setForeground(QBrush(failure_color));
        }

        if (status == RowStatus::None) {
            status_item->setForeground(QBrush());
        }

        status_item->setData(static_cast<int>(status), kStatusRole);
        apply_status_text(status_item);
    }
}


void CategorizationDialog::on_select_all_toggled(bool checked)
{
    apply_select_all(checked);
}

void CategorizationDialog::on_auto_approve_filename_changes_toggled(bool checked)
{
    auto_approve_filename_changes_ = checked;
    if (auto_approve_filename_changes_ || auto_approve_categorization_) {
        apply_auto_approval_to_rows();
    }
}

void CategorizationDialog::on_auto_approve_categorization_toggled(bool checked)
{
    auto_approve_categorization_ = checked;
    if (auto_approve_filename_changes_ || auto_approve_categorization_) {
        apply_auto_approval_to_rows();
    }
}

bool CategorizationDialog::row_is_auto_approval_candidate(int row) const
{
    const auto preview = build_preview_record_for_row(row);
    if (!preview) {
        return false;
    }
    const bool renames = preview->source_file_name != preview->destination_file_name;
    const bool categorizes = !preview->rename_only && !preview->category.empty();
    if (!renames && !categorizes) {
        return false;
    }
    if (renames && !auto_approve_filename_changes_) {
        return false;
    }
    if (categorizes && !auto_approve_categorization_) {
        return false;
    }
    return true;
}

void CategorizationDialog::update_auto_approval_for_row(int row)
{
    if ((!auto_approve_filename_changes_ && !auto_approve_categorization_) ||
        !model || row < 0 || row >= model->rowCount()) {
        return;
    }

    auto* item = model->item(row, ColumnSelect);
    if (!item) {
        return;
    }

    const bool previous_updating = updating_select_all;
    updating_select_all = true;
    item->setCheckState(row_is_auto_approval_candidate(row) ? Qt::Checked : Qt::Unchecked);
    updating_select_all = previous_updating;
    update_select_all_state();
}

void CategorizationDialog::apply_auto_approval_to_rows()
{
    if (!model) {
        return;
    }

    const bool previous_updating = updating_select_all;
    updating_select_all = true;
    int approved_count = 0;
    for (int row = 0; row < model->rowCount(); ++row) {
        auto* item = model->item(row, ColumnSelect);
        if (!item) {
            continue;
        }
        const bool approve = row_is_auto_approval_candidate(row);
        item->setCheckState(approve ? Qt::Checked : Qt::Unchecked);
        if (approve) {
            ++approved_count;
        }
    }
    updating_select_all = previous_updating;
    update_select_all_state();

    if (ui_logger) {
        ui_logger->info("Auto-approved {} of {} review item(s) by operation policy.",
                        approved_count,
                        model->rowCount());
    }
}

std::vector<int> CategorizationDialog::selected_row_indices() const
{
    std::vector<int> rows;
    if (!table_view || !table_view->selectionModel()) {
        return rows;
    }

    const QModelIndexList selected = table_view->selectionModel()->selectedRows();
    rows.reserve(selected.size());
    for (const auto& index : selected) {
        rows.push_back(index.row());
    }
    std::sort(rows.begin(), rows.end());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
    return rows;
}

std::optional<int> CategorizationDialog::current_preview_row() const
{
    if (!model || model->rowCount() == 0) {
        return std::nullopt;
    }

    const auto rows = selected_row_indices();
    if (!rows.empty()) {
        return rows.front();
    }

    if (table_view) {
        const QModelIndex current = table_view->currentIndex();
        if (current.isValid()) {
            return current.row();
        }
    }

    return 0;
}

bool CategorizationDialog::preview_row(int row)
{
    if (!model || row < 0 || row >= model->rowCount()) {
        return false;
    }

    auto* file_item = model->item(row, ColumnFile);
    if (!file_item) {
        return false;
    }

    std::string source_dir = file_item->data(kFilePathRole).toString().toStdString();
    if (source_dir.empty()) {
        source_dir = base_dir_;
    }
    const std::string file_name = file_item->text().toStdString();
    if (source_dir.empty() || file_name.empty()) {
        return false;
    }

    if (!preview_service_) {
        preview_service_ = create_file_preview_service();
    }

    const std::filesystem::path full_path =
        Utils::utf8_to_path(source_dir) / Utils::utf8_to_path(file_name);
    const bool launched = preview_service_->preview_file(full_path, this);
    if (!launched && ui_logger) {
        ui_logger->warn("Preview launch failed for '{}'", Utils::path_to_utf8(full_path));
    }
    return launched;
}

void CategorizationDialog::update_preview_button_state()
{
    if (!preview_button) {
        return;
    }
    preview_button->setEnabled(model && model->rowCount() > 0);
}

void CategorizationDialog::on_preview_button_clicked()
{
    const auto row = current_preview_row();
    if (!row.has_value()) {
        return;
    }
    preview_row(*row);
}

void CategorizationDialog::on_table_double_clicked(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }
    if (index.column() != ColumnFile && index.column() != ColumnType) {
        return;
    }
    preview_row(index.row());
}

void CategorizationDialog::on_select_highlighted_clicked()
{
    const auto rows = selected_row_indices();
    if (rows.empty()) {
        QMessageBox::information(this,
                                 tr("No items selected"),
                                 tr("Highlight one or more rows to select them for processing."));
        return;
    }
    apply_check_state_to_rows(rows, Qt::Checked);
}

void CategorizationDialog::record_move_for_undo(int row,
                                                const std::string& source,
                                                const std::string& destination,
                                                std::uintmax_t size_bytes,
                                                std::time_t mtime,
                                                const std::string& stable_identity,
                                                const std::string& revision_token,
                                                long long history_id)
{
    move_history_.push_back(MoveRecord{
        row,
        source,
        destination,
        size_bytes,
        mtime,
        stable_identity,
        revision_token,
        history_id});
}

void CategorizationDialog::remove_empty_parent_directories(const std::string& destination)
{
    std::filesystem::path dest_path = Utils::utf8_to_path(destination);
    auto parent = dest_path.parent_path();
    while (!parent.empty()) {
        std::error_code ec;
        if (!std::filesystem::exists(parent)) {
            parent = parent.parent_path();
            continue;
        }
        if (std::filesystem::is_directory(parent) &&
            std::filesystem::is_empty(parent, ec) && !ec) {
            std::filesystem::remove(parent, ec);
            parent = parent.parent_path();
        } else {
            break;
        }
    }
}

bool CategorizationDialog::move_file_back(const std::string& source, const std::string& destination)
{
    if (!storage_provider_) {
        if (core_logger) {
            core_logger->error("Undo move failed '{}' -> '{}': no storage provider available",
                               destination,
                               source);
        }
        return false;
    }

    const auto undo_result = storage_provider_->undo_move(source, destination);
    if (!undo_result.success && core_logger) {
        core_logger->error("Undo move failed '{}' -> '{}': {}",
                           destination,
                           source,
                           undo_result.message.empty() ? "unknown error" : undo_result.message);
    }
    return undo_result.success;
}

long long CategorizationDialog::record_review_history(int row,
                                                      const std::string& operation,
                                                      const std::string& source,
                                                      const std::string& destination,
                                                      const std::string& original_file_name,
                                                      const std::string& final_file_name,
                                                      const std::string& category,
                                                      const std::string& subcategory,
                                                      std::uintmax_t size_bytes,
                                                      std::time_t mtime,
                                                      const std::string& stable_identity,
                                                      const std::string& revision_token)
{
    if (!history_store_ || !history_store_->is_open()) {
        return 0;
    }

    ReviewHistoryStore::Entry entry;
    entry.provider_id = storage_provider_ ? storage_provider_->id() : std::string("local_fs");
    entry.operation = ReviewHistoryStore::operation_from_string(operation);
    entry.source_path = source;
    entry.destination_path = destination;
    entry.original_file_name = original_file_name;
    entry.final_file_name = final_file_name;
    entry.category = category;
    entry.subcategory = subcategory;
    entry.file_description = history_description_for_row(row);
    entry.size_bytes = size_bytes;
    entry.mtime = mtime;
    entry.stable_identity = stable_identity;
    entry.revision_token = revision_token;

    std::string error;
    const auto id = history_store_->record_entry(entry, &error);
    if (!id) {
        if (core_logger) {
            core_logger->warn("Failed to record review history for '{}': {}",
                              original_file_name,
                              error);
        }
        return 0;
    }
    return *id;
}

std::string CategorizationDialog::history_description_for_row(int row) const
{
    if (!model || row < 0 || row >= model->rowCount()) {
        return {};
    }
    const auto* file_item = model->item(row, ColumnFile);
    if (!file_item) {
        return {};
    }
    return strip_history_description_label(
        file_item->data(kLearningContextRole).toString().toStdString());
}

bool CategorizationDialog::undo_move_history()
{
    if (move_history_.empty()) {
        return false;
    }

    bool any_success = false;
    for (auto it = move_history_.rbegin(); it != move_history_.rend(); ++it) {
        if (move_file_back(it->source_path, it->destination_path)) {
            if (history_store_ && it->history_id > 0) {
                std::string history_error;
                if (!history_store_->mark_undone(it->history_id, &history_error) && core_logger) {
                    core_logger->warn("Failed to mark review history row {} undone: {}",
                                      it->history_id,
                                      history_error);
                }
            }
            any_success = true;
        }
    }

    if (any_success && core_logger) {
        core_logger->info("Undo completed for {} moved file(s)", move_history_.size());
    }

    return any_success;
}

void CategorizationDialog::update_status_after_undo()
{
    ScopedFlag guard(suppress_item_changed_);
    for (const auto& record : move_history_) {
        update_status_column(record.row_index, false, false);
        if (!model) {
            continue;
        }
        auto* file_item = model->item(record.row_index, ColumnFile);
        if (!file_item) {
            continue;
        }
        const auto source_path = Utils::utf8_to_path(record.source_path);
        const std::string source_name = Utils::path_to_utf8(source_path.filename());
        if (!source_name.empty() && file_item->text().toStdString() != source_name) {
            file_item->setText(QString::fromStdString(source_name));
        }
        update_preview_column(record.row_index);
    }
}


void CategorizationDialog::apply_select_all(bool checked)
{
    updating_select_all = true;
    for (int row = 0; row < model->rowCount(); ++row) {
        if (auto* item = model->item(row, ColumnSelect)) {
            item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
        }
        update_preview_column(row);
    }
    updating_select_all = false;
    update_select_all_state();
}

void CategorizationDialog::apply_check_state_to_rows(const std::vector<int>& rows, Qt::CheckState state)
{
    if (!model) {
        return;
    }
    updating_select_all = true;
    for (int row : rows) {
        if (row < 0 || row >= model->rowCount()) {
            continue;
        }
        if (auto* item = model->item(row, ColumnSelect)) {
            item->setCheckState(state);
        }
        update_preview_column(row);
    }
    updating_select_all = false;
    update_select_all_state();
}

void CategorizationDialog::on_bulk_edit_clicked()
{
    if (!model || !table_view) {
        return;
    }
    if (table_view->isColumnHidden(ColumnCategory)) {
        QMessageBox::information(this,
                                 tr("Bulk edit unavailable"),
                                 tr("Bulk editing categories is unavailable while picture rename-only mode is active."));
        return;
    }
    const auto rows = selected_row_indices();
    if (rows.empty()) {
        QMessageBox::information(this,
                                 tr("No items selected"),
                                 tr("Highlight one or more rows to edit their categories."));
        return;
    }

    const bool allow_subcategory = !table_view->isColumnHidden(ColumnSubcategory);
    BulkEditDialog dialog(allow_subcategory, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const std::string category = dialog.category();
    const std::string subcategory = dialog.subcategory();
    if (category.empty() && subcategory.empty()) {
        return;
    }

    for (int row : rows) {
        bool rename_only = false;
        bool used_consistency_hints = false;
        FileType file_type = FileType::File;
        if (!resolve_row_flags(row, rename_only, used_consistency_hints, file_type)) {
            continue;
        }
        if (rename_only) {
            continue;
        }
        if (!category.empty()) {
            if (auto* category_item = model->item(row, ColumnCategory)) {
                category_item->setText(QString::fromStdString(category));
            }
        }
        if (allow_subcategory && !subcategory.empty()) {
            if (auto* subcategory_item = model->item(row, ColumnSubcategory)) {
                subcategory_item->setText(QString::fromStdString(subcategory));
            }
        }
    }
}

void CategorizationDialog::on_show_subcategories_toggled(bool checked)
{
    show_subcategory_column = checked;
    apply_subcategory_visibility();
    apply_rename_only_row_visibility();
    for (int row = 0; row < model->rowCount(); ++row) {
        update_preview_column(row);
    }
    if (auto_approve_filename_changes_ || auto_approve_categorization_) {
        apply_auto_approval_to_rows();
    }
}

void CategorizationDialog::on_rename_images_only_toggled(bool checked)
{
    if (!model) {
        return;
    }

    ScopedFlag guard(suppress_item_changed_);
    for (int row = 0; row < model->rowCount(); ++row) {
        if (!row_is_supported_image(row)) {
            continue;
        }
        auto* file_item = model->item(row, ColumnFile);
        if (!file_item) {
            continue;
        }
        file_item->setData(checked, kRenameOnlyRole);

        if (auto* category_item = model->item(row, ColumnCategory)) {
            category_item->setEditable(!checked);
            category_item->setIcon(checked ? QIcon() : edit_icon());
        }
        if (auto* subcategory_item = model->item(row, ColumnSubcategory)) {
            subcategory_item->setEditable(!checked);
            subcategory_item->setIcon(checked ? QIcon() : edit_icon());
        }
    }

    ensure_unique_suggested_names_in_model();
    for (int row = 0; row < model->rowCount(); ++row) {
        if (!row_is_supported_image(row)) {
            continue;
        }
        update_preview_column(row);
    }

    dry_run_plan_.clear();
    apply_category_visibility();
    apply_subcategory_visibility();
    apply_rename_only_row_visibility();
    update_subcategory_checkbox_state();
    if (auto_approve_filename_changes_ || auto_approve_categorization_) {
        apply_auto_approval_to_rows();
    }
}

void CategorizationDialog::on_rename_documents_only_toggled(bool checked)
{
    if (!model) {
        return;
    }

    ScopedFlag guard(suppress_item_changed_);
    for (int row = 0; row < model->rowCount(); ++row) {
        if (!row_is_supported_document(row)) {
            continue;
        }
        auto* file_item = model->item(row, ColumnFile);
        if (!file_item) {
            continue;
        }
        file_item->setData(checked, kRenameOnlyRole);

        if (auto* category_item = model->item(row, ColumnCategory)) {
            category_item->setEditable(!checked);
            category_item->setIcon(checked ? QIcon() : edit_icon());
        }
        if (auto* subcategory_item = model->item(row, ColumnSubcategory)) {
            subcategory_item->setEditable(!checked);
            subcategory_item->setIcon(checked ? QIcon() : edit_icon());
        }
    }

    ensure_unique_suggested_names_in_model();
    for (int row = 0; row < model->rowCount(); ++row) {
        if (!row_is_supported_document(row)) {
            continue;
        }
        update_preview_column(row);
    }

    dry_run_plan_.clear();
    apply_category_visibility();
    apply_subcategory_visibility();
    apply_rename_only_row_visibility();
    update_subcategory_checkbox_state();
    if (auto_approve_filename_changes_ || auto_approve_categorization_) {
        apply_auto_approval_to_rows();
    }
}

void CategorizationDialog::apply_subcategory_visibility()
{
    if (table_view) {
        const bool rename_images_active = rename_images_only_checkbox && rename_images_only_checkbox->isChecked();
        const bool rename_documents_active = rename_documents_only_checkbox && rename_documents_only_checkbox->isChecked();
        auto should_hide_row = [&](int row) {
            if (rename_images_active && row_is_supported_image(row)) {
                return true;
            }
            if (rename_documents_active && row_is_supported_document(row)) {
                return true;
            }
            return false;
        };
        auto should_show_category_column = [&]() {
            if (!rename_images_active && !rename_documents_active) {
                return true;
            }
            if (!model) {
                return true;
            }
            for (int row = 0; row < model->rowCount(); ++row) {
                if (should_hide_row(row)) {
                    continue;
                }
                auto* item = model->item(row, ColumnCategory);
                if (item && !item->text().trimmed().isEmpty()) {
                    return true;
                }
                if (item && item->data(kHiddenCategoryRole).isValid() &&
                    !item->data(kHiddenCategoryRole).toString().trimmed().isEmpty()) {
                    return true;
                }
            }
            return false;
        };

        const bool show_category_column = should_show_category_column();
        const bool hide_subcategory = !show_subcategory_column || !show_category_column;
        table_view->setColumnHidden(ColumnSubcategory, hide_subcategory);
        table_view->setColumnHidden(ColumnPreview, false);
        if (model) {
            auto update_item = [](QStandardItem* item, int role, bool hide) {
                if (!item) {
                    return;
                }
                if (hide) {
                    if (!item->data(role).isValid() && !item->text().trimmed().isEmpty()) {
                        item->setData(item->text(), role);
                    }
                    item->setText(QString());
                } else if (item->data(role).isValid()) {
                    item->setText(item->data(role).toString());
                    item->setData(QVariant(), role);
                }
            };
            for (int row = 0; row < model->rowCount(); ++row) {
                const bool hide_row = show_subcategory_column && show_category_column &&
                                      (rename_images_active || rename_documents_active) &&
                                      should_hide_row(row);
                update_item(model->item(row, ColumnSubcategory), kHiddenSubcategoryRole, hide_row);
            }
        }
    }
}

void CategorizationDialog::apply_category_visibility()
{
    if (table_view) {
        const bool rename_images_active = rename_images_only_checkbox && rename_images_only_checkbox->isChecked();
        const bool rename_documents_active = rename_documents_only_checkbox && rename_documents_only_checkbox->isChecked();
        auto should_hide_row = [&](int row) {
            if (rename_images_active && row_is_supported_image(row)) {
                return true;
            }
            if (rename_documents_active && row_is_supported_document(row)) {
                return true;
            }
            return false;
        };
        bool show_category_column = true;
        if ((rename_images_active || rename_documents_active) && model) {
            show_category_column = false;
            for (int row = 0; row < model->rowCount(); ++row) {
                if (should_hide_row(row)) {
                    continue;
                }
                auto* item = model->item(row, ColumnCategory);
                if (item && !item->text().trimmed().isEmpty()) {
                    show_category_column = true;
                    break;
                }
                if (item && item->data(kHiddenCategoryRole).isValid() &&
                    !item->data(kHiddenCategoryRole).toString().trimmed().isEmpty()) {
                    show_category_column = true;
                    break;
                }
            }
        }
        table_view->setColumnHidden(ColumnCategory, !show_category_column);
        if (bulk_edit_button) {
            bulk_edit_button->setEnabled(show_category_column);
        }
        if (model) {
            auto update_item = [](QStandardItem* item, int role, bool hide) {
                if (!item) {
                    return;
                }
                if (hide) {
                    if (!item->data(role).isValid() && !item->text().trimmed().isEmpty()) {
                        item->setData(item->text(), role);
                    }
                    item->setText(QString());
                } else if (item->data(role).isValid()) {
                    item->setText(item->data(role).toString());
                    item->setData(QVariant(), role);
                }
            };
            for (int row = 0; row < model->rowCount(); ++row) {
                const bool hide_row = show_category_column &&
                                      (rename_images_active || rename_documents_active) &&
                                      should_hide_row(row);
                update_item(model->item(row, ColumnCategory), kHiddenCategoryRole, hide_row);
            }
        }
    }
}

void CategorizationDialog::apply_rename_only_row_visibility()
{
    if (!table_view || !model) {
        return;
    }

    for (int row = 0; row < model->rowCount(); ++row) {
        bool hide_row = false;
        if (rename_images_only_checkbox &&
            rename_images_only_checkbox->isChecked() &&
            row_is_supported_image(row) &&
            row_is_already_renamed_with_category(row)) {
            hide_row = true;
        }
        if (rename_documents_only_checkbox &&
            rename_documents_only_checkbox->isChecked() &&
            row_is_supported_document(row) &&
            row_is_already_renamed_with_category(row)) {
            hide_row = true;
        }
        table_view->setRowHidden(row, hide_row);
    }
}

void CategorizationDialog::apply_rename_visibility()
{
    if (table_view) {
        table_view->setColumnHidden(ColumnSuggestedName, !show_rename_column);
    }
}

void CategorizationDialog::update_rename_only_checkbox_state()
{
    if (!rename_images_only_checkbox) {
        return;
    }

    bool has_images = false;
    bool all_rename_only = true;
    for (const auto& file : categorized_files) {
        if (!is_supported_image_entry(file.file_path, file.file_name, file.type)) {
            continue;
        }
        has_images = true;
        if (!file.rename_only) {
            all_rename_only = false;
        }
    }

    QSignalBlocker blocker(rename_images_only_checkbox);
    const bool enable_images_checkbox = has_images && allow_image_renames_;
    rename_images_only_checkbox->setEnabled(enable_images_checkbox);
    rename_images_only_checkbox->setChecked(enable_images_checkbox && all_rename_only);

    if (rename_documents_only_checkbox) {
        bool has_documents = false;
        bool all_doc_rename_only = true;
        for (const auto& file : categorized_files) {
            if (!is_supported_document_entry(file.file_path, file.file_name, file.type)) {
                continue;
            }
            has_documents = true;
            if (!file.rename_only) {
                all_doc_rename_only = false;
            }
        }

        QSignalBlocker doc_blocker(rename_documents_only_checkbox);
        const bool enable_documents_checkbox = has_documents && allow_document_renames_;
        rename_documents_only_checkbox->setEnabled(enable_documents_checkbox);
        rename_documents_only_checkbox->setChecked(enable_documents_checkbox && all_doc_rename_only);
    }

    update_subcategory_checkbox_state();
}

void CategorizationDialog::update_subcategory_checkbox_state()
{
    if (!show_subcategories_checkbox || !model) {
        return;
    }

    const bool rename_only_active =
        (rename_images_only_checkbox && rename_images_only_checkbox->isChecked()) ||
        (rename_documents_only_checkbox && rename_documents_only_checkbox->isChecked());
    const bool enable_checkbox = !rename_only_active;

    QSignalBlocker blocker(show_subcategories_checkbox);
    show_subcategories_checkbox->setEnabled(enable_checkbox);
    if (!enable_checkbox) {
        show_subcategory_column = false;
        show_subcategories_checkbox->setChecked(false);
        apply_subcategory_visibility();
    }
}

bool CategorizationDialog::row_is_supported_image(int row) const
{
    if (!model || row < 0 || row >= model->rowCount()) {
        return false;
    }
    auto* file_item = model->item(row, ColumnFile);
    if (!file_item) {
        return false;
    }
    const std::string file_name = file_item->text().toStdString();
    const std::string file_path = file_item->data(kFilePathRole).toString().toStdString();
    const FileType file_type = static_cast<FileType>(file_item->data(kFileTypeRole).toInt());
    return is_supported_image_entry(file_path, file_name, file_type);
}

bool CategorizationDialog::row_is_supported_document(int row) const
{
    if (!model || row < 0 || row >= model->rowCount()) {
        return false;
    }
    auto* file_item = model->item(row, ColumnFile);
    if (!file_item) {
        return false;
    }
    const std::string file_name = file_item->text().toStdString();
    const std::string file_path = file_item->data(kFilePathRole).toString().toStdString();
    const FileType file_type = static_cast<FileType>(file_item->data(kFileTypeRole).toInt());
    return is_supported_document_entry(file_path, file_name, file_type);
}

bool CategorizationDialog::row_is_already_renamed_with_category(int row) const
{
    if (!model || row < 0 || row >= model->rowCount()) {
        return false;
    }
    auto* file_item = model->item(row, ColumnFile);
    if (!file_item) {
        return false;
    }
    if (!file_item->data(kRenameLockedRole).toBool()) {
        return false;
    }

    auto* category_item = model->item(row, ColumnCategory);
    std::string category = category_item ? category_item->text().toStdString() : std::string();
    if (is_missing_category_label(category)) {
        category.clear();
    }
    if (category.empty()) {
        return false;
    }
    if (!show_subcategory_column) {
        return true;
    }

    auto* subcategory_item = model->item(row, ColumnSubcategory);
    std::string subcategory = subcategory_item ? subcategory_item->text().toStdString() : std::string();
    if (is_missing_category_label(subcategory)) {
        subcategory.clear();
    }
    return !subcategory.empty();
}

std::optional<std::string> CategorizationDialog::compute_preview_path(int row) const
{
    auto rec = build_preview_record_for_row(row);
    if (rec) {
        return rec->destination;
    }
    return std::nullopt;
}

std::optional<CategorizationDialog::PreviewRecord>
CategorizationDialog::build_preview_record_for_row(int row, std::string* debug_reason) const
{
    auto fail = [&](std::string reason) -> std::optional<PreviewRecord> {
        if (debug_reason) {
            *debug_reason = std::move(reason);
        }
        return std::nullopt;
    };

    if (!model || row < 0 || row >= model->rowCount()) {
        return fail("Invalid model or row");
    }
    if (base_dir_.empty()) {
        return fail("Base dir empty");
    }

    const auto* file_item = model->item(row, ColumnFile);
    const auto* category_item = model->item(row, ColumnCategory);
    const auto* subcategory_item = model->item(row, ColumnSubcategory);
    const auto* rename_item = model->item(row, ColumnSuggestedName);
    if (!file_item || !category_item) {
        return fail("Missing file/category item");
    }

    const std::string file_name = file_item->text().toStdString();
    std::string source_dir = file_item->data(kFilePathRole).toString().toStdString();
    if (source_dir.empty()) {
        source_dir = base_dir_;
    }
    const std::string rename_candidate = rename_item ? rename_item->text().toStdString() : std::string();
    const std::string destination_name = resolve_destination_name(file_name, rename_candidate);
    const bool rename_active = destination_name != file_name;
    bool rename_only = false;
    bool used_consistency_hints = false;
    FileType file_type = FileType::File;
    if (!resolve_row_flags(row, rename_only, used_consistency_hints, file_type)) {
        return fail("Missing row metadata");
    }
    (void)used_consistency_hints;
    (void)file_type;
    if (rename_active) {
        std::string rename_error;
        if (!validate_filename(destination_name, rename_error)) {
            return fail("Invalid rename: " + rename_error);
        }
    }

    if (rename_only) {
        const auto source_path = Utils::utf8_to_path(source_dir) / Utils::utf8_to_path(file_name);
        const auto destination_path = Utils::utf8_to_path(source_dir) / Utils::utf8_to_path(destination_name);
        return PreviewRecord{
            Utils::path_to_utf8(source_path),
            Utils::path_to_utf8(destination_path),
            file_name,
            destination_name,
            std::string(),
            std::string(),
            false,
            true};
    }

    const std::string category = read_item_or_hidden_text(category_item, kHiddenCategoryRole);
    const std::string subcategory = read_item_or_hidden_text(subcategory_item, kHiddenSubcategoryRole);
    const std::string effective_subcategory = subcategory.empty() ? category : subcategory;

    std::string validation_error;
    const bool allow_identical = !show_subcategory_column;
    if (!validate_labels(category, effective_subcategory, validation_error, allow_identical)) {
        return fail("Validation failed: " + validation_error);
    }

    try {
        MovableCategorizedFile categorized_file(*storage_provider_,
                                                source_dir,
                                                base_dir_,
                                                category,
                                                effective_subcategory,
                                                file_name,
                                                destination_name);
        const auto preview_paths = categorized_file.preview_move_paths(show_subcategory_column);
        return PreviewRecord{
            preview_paths.source,
            preview_paths.destination,
            file_name,
            destination_name,
            category,
            effective_subcategory,
            show_subcategory_column,
            false};
    } catch (...) {
        return fail("Exception building preview record");
    }
}

std::string CategorizationDialog::resolve_destination_name(const std::string& original_name,
                                                           const std::string& rename_candidate) const
{
    std::string trimmed = trim_copy(rename_candidate);
    if (trimmed.empty()) {
        return original_name;
    }
    if (trimmed == original_name) {
        return original_name;
    }

    const std::filesystem::path original_path = Utils::utf8_to_path(original_name);
    const std::filesystem::path candidate_path = Utils::utf8_to_path(trimmed);
    if (!candidate_path.has_extension() && original_path.has_extension()) {
        return Utils::path_to_utf8(candidate_path) + original_path.extension().string();
    }

    return trimmed;
}

bool CategorizationDialog::validate_filename(const std::string& name, std::string& error) const
{
    return ReviewNameValidator::validate_filename(name, error);
}

bool CategorizationDialog::resolve_row_flags(int row,
                                             bool& rename_only,
                                             bool& used_consistency_hints,
                                             FileType& file_type) const
{
    if (!model || row < 0 || row >= model->rowCount()) {
        return false;
    }
    auto* file_item = model->item(row, ColumnFile);
    if (!file_item) {
        return false;
    }

    rename_only = file_item->data(kRenameOnlyRole).toBool();
    used_consistency_hints = file_item->data(kUsedConsistencyRole).toBool();
    file_type = static_cast<FileType>(file_item->data(kFileTypeRole).toInt());
    if (!rename_only && (row_is_supported_image(row) || row_is_supported_document(row))) {
        auto* category_item = model->item(row, ColumnCategory);
        if (category_item) {
            const std::string category_text = category_item->text().toStdString();
            if (is_missing_category_label(category_text)) {
                rename_only = true;
            }
        }
    }
    return true;
}

void CategorizationDialog::update_preview_column(int row)
{
    if (!model || row < 0 || row >= model->rowCount()) {
        return;
    }
    auto* preview_item = model->item(row, ColumnPreview);
    if (!preview_item) {
        return;
    }
    if (rename_images_only_checkbox &&
        rename_images_only_checkbox->isChecked() &&
        row_is_supported_image(row)) {
        preview_item->setText(QStringLiteral("-"));
        preview_item->setToolTip(QString());
        return;
    }
    if (rename_documents_only_checkbox &&
        rename_documents_only_checkbox->isChecked() &&
        row_is_supported_document(row)) {
        preview_item->setText(QStringLiteral("-"));
        preview_item->setToolTip(QString());
        return;
    }
    const auto preview = compute_preview_path(row);
    if (preview) {
        std::string display = *preview;
#ifdef _WIN32
        std::replace(display.begin(), display.end(), '/', '\\');
#endif
        preview_item->setText(QString::fromStdString(display));
        preview_item->setToolTip(QString::fromStdString(display));
    } else {
        preview_item->setText(QStringLiteral("-"));
        preview_item->setToolTip(QString());
    }
}

void CategorizationDialog::set_preview_status(int row, const std::string& destination)
{
    if (!model || row < 0 || row >= model->rowCount()) {
        return;
    }
    if (auto* status_item = model->item(row, ColumnStatus)) {
        status_item->setData(static_cast<int>(RowStatus::Preview), kStatusRole);
        status_item->setText(tr("Preview"));
        status_item->setForeground(QBrush(palette().color(QPalette::Link)));
        std::string display = destination;
#ifdef _WIN32
        std::replace(display.begin(), display.end(), '/', '\\');
#endif
        status_item->setToolTip(QString::fromStdString(display));
    }
}

void CategorizationDialog::persist_move_plan()
{
    if (undo_dir_.empty() || base_dir_.empty() || move_history_.empty()) {
        return;
    }

    std::vector<UndoManager::Entry> entries;
    entries.reserve(move_history_.size());
    for (const auto& rec : move_history_) {
        entries.push_back(UndoManager::Entry{
            rec.source_path,
            rec.destination_path,
            rec.size_bytes,
            rec.mtime,
            rec.stable_identity,
            rec.revision_token});
    }

    UndoManager manager(undo_dir_);
    manager.save_plan(base_dir_,
                      storage_provider_ ? storage_provider_->id() : std::string("local_fs"),
                      entries,
                      core_logger);
}

void CategorizationDialog::clear_move_history()
{
    move_history_.clear();
}

void CategorizationDialog::retranslate_ui()
{
    setWindowTitle(tr("Review and Confirm"));

    const auto set_text_if = [](auto* widget, const QString& text) {
        if (widget) {
            widget->setText(text);
        }
    };

    set_text_if(select_all_checkbox, tr("Select all"));
    set_text_if(auto_approve_filenames_checkbox, tr("Auto-approve filename changes"));
    set_text_if(auto_approve_categories_checkbox, tr("Auto-approve categorization"));
    set_text_if(select_highlighted_button, tr("Select highlighted"));
    set_text_if(bulk_edit_button, tr("Edit selected..."));
    set_text_if(show_subcategories_checkbox, tr("Create subcategory folders"));
    set_text_if(dry_run_checkbox, tr("Dry run (preview only, do not move files)"));
    set_text_if(rename_images_only_checkbox, tr("Do not categorize picture files (only rename)"));
    set_text_if(rename_documents_only_checkbox, tr("Do not categorize document files (only rename)"));
    set_text_if(preview_button, tr("Preview"));
    set_text_if(confirm_button, tr("Confirm and Process"));
    set_text_if(continue_button, tr("Continue Later"));
    set_text_if(undo_button, tr("Undo this change"));
    set_text_if(close_button, tr("Close"));

    if (select_highlighted_button) {
        select_highlighted_button->setToolTip(tr("Mark highlighted rows for processing (Ctrl+Space)."));
    }
    if (auto_approve_filenames_checkbox) {
        auto_approve_filenames_checkbox->setToolTip(
            tr("Preselect rows with filename changes. Rows that also categorize require categorization auto-approval too."));
    }
    if (auto_approve_categories_checkbox) {
        auto_approve_categories_checkbox->setToolTip(
            tr("Preselect rows with valid categorization. Rows that also rename require filename auto-approval too."));
    }
    if (bulk_edit_button) {
        bulk_edit_button->setToolTip(tr("Apply category/subcategory values to highlighted rows."));
    }

    if (model) {
        model->setHorizontalHeaderLabels(QStringList{
            tr("Process"),
            tr("File"),
            tr("Type"),
            tr("Suggested filename"),
            tr("Category"),
            tr("Subcategory"),
            tr("Status"),
            tr("Destination")
        });

        for (int row = 0; row < model->rowCount(); ++row) {
            if (auto* type_item = model->item(row, ColumnType)) {
                update_type_icon(type_item);
                type_item->setTextAlignment(Qt::AlignCenter);
            }
            if (auto* status_item = model->item(row, ColumnStatus)) {
                apply_status_text(status_item);
            }
        }
    }
}

void CategorizationDialog::apply_status_text(QStandardItem* item) const
{
    if (!item) {
        return;
    }

    switch (status_from_item(item)) {
    case RowStatus::Moved:
        item->setText(tr("Moved"));
        break;
    case RowStatus::Renamed:
        item->setText(tr("Renamed"));
        break;
    case RowStatus::RenamedAndMoved:
        item->setText(tr("Renamed & Moved"));
        break;
    case RowStatus::Skipped:
        item->setText(tr("Skipped"));
        break;
    case RowStatus::Preview:
        item->setText(tr("Preview"));
        break;
    case RowStatus::NotSelected:
        item->setText(tr("Not selected"));
        break;
    case RowStatus::None:
    default:
        item->setText(QString());
        break;
    }
}

CategorizationDialog::RowStatus CategorizationDialog::status_from_item(const QStandardItem* item) const
{
    if (!item) {
        return RowStatus::None;
    }

    bool ok = false;
    const int value = item->data(kStatusRole).toInt(&ok);
    if (!ok) {
        return RowStatus::None;
    }

    const RowStatus status = static_cast<RowStatus>(value);
    switch (status) {
    case RowStatus::None:
    case RowStatus::Moved:
    case RowStatus::Renamed:
    case RowStatus::RenamedAndMoved:
    case RowStatus::Skipped:
    case RowStatus::NotSelected:
    case RowStatus::Preview:
        return status;
    }

    return RowStatus::None;
}


void CategorizationDialog::on_item_changed(QStandardItem* item)
{
    if (!item || updating_select_all || suppress_item_changed_) {
        return;
    }

    if (item->column() == ColumnSelect) {
        update_select_all_state();
    } else if (item->column() == ColumnCategory ||
               item->column() == ColumnSubcategory ||
               item->column() == ColumnSuggestedName) {
        if ((item->column() == ColumnCategory || item->column() == ColumnSubcategory) &&
            (row_is_supported_image(item->row()) || row_is_supported_document(item->row()))) {
            const std::string text = item->text().toStdString();
            if (is_missing_category_label(text)) {
                ScopedFlag guard(suppress_item_changed_);
                item->setText(QString());
            }
        }
        update_preview_column(item->row());
        update_auto_approval_for_row(item->row());
        if (item->column() == ColumnCategory || item->column() == ColumnSubcategory) {
            update_subcategory_checkbox_state();
        }
    }
    // invalidate preview plan only on user-facing edits (selection/category fields)
    if (item->column() == ColumnSelect ||
        item->column() == ColumnCategory ||
        item->column() == ColumnSubcategory ||
        item->column() == ColumnSuggestedName) {
        dry_run_plan_.clear();
    }
}


void CategorizationDialog::update_select_all_state()
{
    if (!select_all_checkbox) {
        return;
    }

    bool all_checked = true;
    for (int row = 0; row < model->rowCount(); ++row) {
        if (auto* item = model->item(row, ColumnSelect)) {
            if (item->checkState() != Qt::Checked) {
                all_checked = false;
                break;
            }
        }
    }

    QSignalBlocker blocker(select_all_checkbox);
    select_all_checkbox->setChecked(all_checked);
}

void CategorizationDialog::changeEvent(QEvent* event)
{
    QDialog::changeEvent(event);
    if (!event) {
        return;
    }

    if (event->type() == QEvent::LanguageChange) {
        retranslate_ui();
        for (int row = 0; row < model->rowCount(); ++row) {
            update_preview_column(row);
        }
        return;
    }

    if (event->type() == QEvent::PaletteChange ||
        event->type() == QEvent::ApplicationPaletteChange) {
        apply_theme_styles();
    }
}


void CategorizationDialog::closeEvent(QCloseEvent* event)
{
    record_categorization_to_db();
    QDialog::closeEvent(event);
}
void CategorizationDialog::set_show_subcategory_column(bool enabled)
{
    if (show_subcategory_column == enabled) {
        return;
    }
    show_subcategory_column = enabled;
    if (show_subcategories_checkbox) {
        QSignalBlocker blocker(show_subcategories_checkbox);
        show_subcategories_checkbox->setChecked(enabled);
    }
    apply_subcategory_visibility();
    update_subcategory_checkbox_state();
}

void CategorizationDialog::set_show_rename_column(bool enabled)
{
    if (show_rename_column == enabled) {
        return;
    }
    show_rename_column = enabled;
    apply_rename_visibility();
}
#ifdef AI_FILE_SORTER_TEST_BUILD
void CategorizationDialog::test_set_entries(const std::vector<CategorizedFile>& files) {
    categorized_files = files;
    include_subdirectories_ = false;
    base_dir_.clear();
    if (!categorized_files.empty()) {
        base_dir_ = categorized_files.front().file_path;
    }
    ensure_unique_suggested_names(categorized_files, base_dir_, show_subcategory_column);
    set_show_rename_column(std::any_of(categorized_files.begin(),
                                       categorized_files.end(),
                                       [](const CategorizedFile& file) {
                                           return !file.suggested_name.empty();
                                       }));
    update_rename_only_checkbox_state();
    apply_category_visibility();
    apply_subcategory_visibility();
    populate_model();
    const bool auto_approve_filenames_after_init = auto_approve_filename_changes_;
    const bool auto_approve_categories_after_init = auto_approve_categorization_;
    auto_approve_filename_changes_ = false;
    auto_approve_categorization_ = false;
    on_rename_images_only_toggled(rename_images_only_checkbox && rename_images_only_checkbox->isChecked());
    on_rename_documents_only_toggled(rename_documents_only_checkbox && rename_documents_only_checkbox->isChecked());
    auto_approve_filename_changes_ = auto_approve_filenames_after_init;
    auto_approve_categorization_ = auto_approve_categories_after_init;
    update_subcategory_checkbox_state();
    if (auto_approve_filename_changes_ || auto_approve_categorization_) {
        apply_auto_approval_to_rows();
    }
}

void CategorizationDialog::test_trigger_confirm() {
    on_confirm_and_sort_button_clicked();
}

bool CategorizationDialog::test_trigger_preview(int row) {
    return preview_row(row);
}

void CategorizationDialog::test_trigger_undo() {
    on_undo_button_clicked();
}

bool CategorizationDialog::test_undo_enabled() const {
    return undo_button && undo_button->isEnabled();
}
#endif
