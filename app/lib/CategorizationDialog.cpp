#include "CategorizationDialog.hpp"

#include "DatabaseManager.hpp"
#include "Logger.hpp"
#include "MovableCategorizedFile.hpp"
#include "TestHooks.hpp"
#include "Utils.hpp"
#include "UndoManager.hpp"
#include "DryRunPreviewDialog.hpp"
#include "DocumentTextAnalyzer.hpp"
#include "LlavaImageAnalyzer.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QStyle>
#include <QBrush>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QPushButton>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
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
#include <QScreen>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
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

struct ScopedFlag {
    bool& ref;
    explicit ScopedFlag(bool& target) : ref(target) { ref = true; }
    ~ScopedFlag() { ref = false; }
};

void ensure_unique_image_suggested_names(std::vector<CategorizedFile>& files,
                                         const std::string& base_dir,
                                         bool use_subcategory);

QString edit_icon_html(int size = 16);

std::string to_lower_copy_str(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string trim_copy(const std::string& value) {
    std::string result = value;
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    result.erase(result.begin(), std::find_if(result.begin(), result.end(), not_space));
    result.erase(std::find_if(result.rbegin(), result.rend(), not_space).base(), result.end());
    return result;
}

bool is_missing_category_label(const std::string& value) {
    const std::string trimmed = trim_copy(value);
    if (trimmed.empty()) {
        return true;
    }
    return to_lower_copy_str(trimmed) == "uncategorized";
}

// Dialog for bulk editing category and subcategory values.
class BulkEditDialog final : public QDialog {
public:
    explicit BulkEditDialog(bool allow_subcategory, QWidget* parent = nullptr)
        : QDialog(parent),
          allow_subcategory_(allow_subcategory) {
        setWindowTitle(QObject::tr("Edit selected items"));

        auto* layout = new QVBoxLayout(this);
        auto* form_layout = new QFormLayout();

        category_edit_ = new QLineEdit(this);
        category_edit_->setPlaceholderText(QObject::tr("Leave empty to keep existing"));
        form_layout->addRow(QObject::tr("Category"), category_edit_);

        if (allow_subcategory_) {
            subcategory_edit_ = new QLineEdit(this);
            subcategory_edit_->setPlaceholderText(QObject::tr("Leave empty to keep existing"));
            form_layout->addRow(QObject::tr("Subcategory"), subcategory_edit_);
        }

        layout->addLayout(form_layout);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        ok_button_ = buttons->button(QDialogButtonBox::Ok);
        if (ok_button_) {
            ok_button_->setEnabled(false);
        }
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(category_edit_, &QLineEdit::textChanged, this, &BulkEditDialog::update_ok_state);
        if (subcategory_edit_) {
            connect(subcategory_edit_, &QLineEdit::textChanged, this, &BulkEditDialog::update_ok_state);
        }

        layout->addWidget(buttons);
        update_ok_state();
    }

    std::string category() const {
        return category_edit_ ? category_edit_->text().trimmed().toStdString() : std::string();
    }

    std::string subcategory() const {
        if (!allow_subcategory_ || !subcategory_edit_) {
            return std::string();
        }
        return subcategory_edit_->text().trimmed().toStdString();
    }

private:
    void update_ok_state() {
        const bool has_category = category_edit_ && !category_edit_->text().trimmed().isEmpty();
        const bool has_subcategory = allow_subcategory_ && subcategory_edit_ &&
                                     !subcategory_edit_->text().trimmed().isEmpty();
        if (ok_button_) {
            ok_button_->setEnabled(has_category || has_subcategory);
        }
    }

    QLineEdit* category_edit_{nullptr};
    QLineEdit* subcategory_edit_{nullptr};
    QPushButton* ok_button_{nullptr};
    bool allow_subcategory_{false};
};

bool contains_only_allowed_chars(const std::string& value) {
    for (unsigned char ch : value) {
        if (std::iscntrl(ch)) {
            return false;
        }
        static const std::string forbidden = R"(<>:"/\|?*)";
        if (forbidden.find(static_cast<char>(ch)) != std::string::npos) {
            return false;
        }
        // Everything else is allowed (including non-ASCII letters and punctuation).
    }
    return true;
}

bool has_leading_or_trailing_space_or_dot(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    const unsigned char first = static_cast<unsigned char>(value.front());
    const unsigned char last = static_cast<unsigned char>(value.back());
    return std::isspace(first) || std::isspace(last) || value.front() == '.' || value.back() == '.';
}

bool is_reserved_windows_name(const std::string& value) {
    static const std::vector<std::string> reserved = {
        "con","prn","aux","nul",
        "com1","com2","com3","com4","com5","com6","com7","com8","com9",
        "lpt1","lpt2","lpt3","lpt4","lpt5","lpt6","lpt7","lpt8","lpt9"
    };
    const std::string lower = to_lower_copy_str(value);
    return std::find(reserved.begin(), reserved.end(), lower) != reserved.end();
}

bool looks_like_extension_label(const std::string& value) {
    const auto dot_pos = value.rfind('.');
    if (dot_pos == std::string::npos || dot_pos == value.size() - 1) {
        return false;
    }
    const std::string ext = value.substr(dot_pos + 1);
    if (ext.empty() || ext.size() > 5) {
        return false;
    }
    return std::all_of(ext.begin(), ext.end(), [](unsigned char ch) { return std::isalpha(ch); });
}

bool validate_labels(const std::string& category,
                     const std::string& subcategory,
                     std::string& error,
                     bool allow_identical = false) {
    constexpr size_t kMaxLabelLength = 80;
    if (category.empty() || subcategory.empty()) {
        error = "Category or subcategory is empty";
        return false;
    }
    if (category.size() > kMaxLabelLength || subcategory.size() > kMaxLabelLength) {
        error = "Category or subcategory exceeds max length";
        return false;
    }
    if (!contains_only_allowed_chars(category) || !contains_only_allowed_chars(subcategory)) {
        error = "Category or subcategory contains disallowed characters";
        return false;
    }
    if (looks_like_extension_label(category) || looks_like_extension_label(subcategory)) {
        error = "Category or subcategory looks like a file extension";
        return false;
    }
    if (is_reserved_windows_name(category) || is_reserved_windows_name(subcategory)) {
        error = "Category or subcategory is a reserved name";
        return false;
    }
    if (!allow_identical && to_lower_copy_str(category) == to_lower_copy_str(subcategory)) {
        error = "Category and subcategory are identical";
        return false;
    }
    return true;
}

std::chrono::system_clock::time_point to_system_clock(std::filesystem::file_time_type file_time) {
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
    return std::chrono::clock_cast<std::chrono::system_clock>(file_time);
#else
    const auto now = decltype(file_time)::clock::now();
    const auto delta = std::chrono::duration_cast<std::chrono::system_clock::duration>(file_time - now);
    return std::chrono::system_clock::now() + delta;
#endif
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
                                           QWidget* parent)
    : QDialog(parent),
      db_manager(db_manager),
      show_subcategory_column(show_subcategory_col),
      core_logger(Logger::get_logger("core_logger")),
      db_logger(Logger::get_logger("db_logger")),
      ui_logger(Logger::get_logger("ui_logger")),
      undo_dir_(undo_dir)
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


bool CategorizationDialog::is_dialog_valid() const
{
    return model != nullptr && table_view != nullptr;
}


void CategorizationDialog::show_results(const std::vector<CategorizedFile>& files,
                                        const std::string& base_dir_override,
                                        bool include_subdirectories,
                                        bool allow_image_renames,
                                        bool allow_document_renames)
{
    categorized_files = files;
    include_subdirectories_ = include_subdirectories;
    allow_image_renames_ = allow_image_renames;
    allow_document_renames_ = allow_document_renames;
    base_dir_.clear();
    if (!base_dir_override.empty()) {
        base_dir_ = base_dir_override;
    } else if (!categorized_files.empty()) {
        base_dir_ = categorized_files.front().file_path;
    }
    ensure_unique_image_suggested_names(categorized_files, base_dir_, show_subcategory_column);
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
    on_rename_images_only_toggled(rename_images_only_checkbox && rename_images_only_checkbox->isChecked());
    on_rename_documents_only_toggled(rename_documents_only_checkbox && rename_documents_only_checkbox->isChecked());
    update_subcategory_checkbox_state();
    exec();
}


void CategorizationDialog::setup_ui()
{
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
    select_highlighted_button = new QPushButton(this);
    bulk_edit_button = new QPushButton(this);

    select_layout->addWidget(select_all_checkbox);
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
    table_view->setModel(model);
    table_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_view->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
    table_view->horizontalHeader()->setStretchLastSection(true);
    table_view->verticalHeader()->setVisible(false);
    table_view->horizontalHeader()->setSectionsClickable(true);
    table_view->horizontalHeader()->setSortIndicatorShown(true);
    table_view->setSortingEnabled(true);
    table_view->setColumnHidden(ColumnType, false);
    table_view->setColumnHidden(ColumnSuggestedName, !show_rename_column);
    table_view->setColumnHidden(ColumnSubcategory, !show_subcategory_column);
    table_view->setColumnHidden(ColumnPreview, false);
    table_view->setColumnWidth(ColumnSelect, 70);
    table_view->setIconSize(QSize(16, 16));
    table_view->setColumnWidth(ColumnType, table_view->iconSize().width() + 12);
    scroll_layout->addWidget(table_view, 1);

    auto* tip_label = new QLabel(this);
    tip_label->setWordWrap(true);
    QFont tip_font = tip_label->font();
    tip_font.setItalic(true);
    tip_label->setFont(tip_font);
    tip_label->setTextFormat(Qt::RichText);
    tip_label->setText(tr("Tip: Click %1 cells to rename them.")
                           .arg(edit_icon_html()));
    scroll_layout->addWidget(tip_label);

    confirm_button = new QPushButton(this);
    continue_button = new QPushButton(this);
    undo_button = new QPushButton(this);
    undo_button->setEnabled(false);
    undo_button->setVisible(false);
    close_button = new QPushButton(this);
    close_button->setVisible(false);

    scroll_area->setWidget(scroll_widget);
    layout->addWidget(scroll_area, 1);

    auto* bottom_layout = new QHBoxLayout();
    bottom_layout->setContentsMargins(0, 0, 0, 0);
    bottom_layout->setSpacing(8);
    bottom_layout->addStretch(1);
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
    connect(select_all_checkbox, &QCheckBox::toggled, this, &CategorizationDialog::on_select_all_toggled);
    connect(select_highlighted_button, &QPushButton::clicked, this, &CategorizationDialog::on_select_highlighted_clicked);
    connect(bulk_edit_button, &QPushButton::clicked, this, &CategorizationDialog::on_bulk_edit_clicked);
    connect(model, &QStandardItemModel::itemChanged, this, &CategorizationDialog::on_item_changed);
    connect(show_subcategories_checkbox, &QCheckBox::toggled,
            this, &CategorizationDialog::on_show_subcategories_toggled);
    connect(rename_images_only_checkbox, &QCheckBox::toggled,
            this, &CategorizationDialog::on_rename_images_only_toggled);
    connect(rename_documents_only_checkbox, &QCheckBox::toggled,
            this, &CategorizationDialog::on_rename_documents_only_toggled);

    auto* select_highlighted_shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Space), this);
    connect(select_highlighted_shortcut, &QShortcut::activated,
            this, &CategorizationDialog::on_select_highlighted_clicked);
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

bool is_supported_image_entry(const std::string& file_path,
                              const std::string& file_name,
                              FileType file_type)
{
    if (file_type != FileType::File) {
        return false;
    }
    const auto full_path = Utils::utf8_to_path(file_path) / Utils::utf8_to_path(file_name);
    return LlavaImageAnalyzer::is_supported_image(full_path);
}

bool is_supported_document_entry(const std::string& file_path,
                                 const std::string& file_name,
                                 FileType file_type)
{
    if (file_type != FileType::File) {
        return false;
    }
    const auto full_path = Utils::utf8_to_path(file_path) / Utils::utf8_to_path(file_name);
    return DocumentTextAnalyzer::is_supported_document(full_path);
}

std::filesystem::path build_suggested_target_dir(const CategorizedFile& file,
                                                 const std::string& base_dir_override,
                                                 bool use_subcategory)
{
    const auto source_dir = Utils::utf8_to_path(file.file_path);
    const auto base_dir = base_dir_override.empty()
        ? source_dir
        : Utils::utf8_to_path(base_dir_override);
    if (file.rename_only || file.category.empty()) {
        return source_dir;
    }

    const auto category = Utils::utf8_to_path(file.category);
    if (use_subcategory && !file.subcategory.empty()) {
        const auto subcategory = Utils::utf8_to_path(file.subcategory);
        return base_dir / category / subcategory;
    }
    return base_dir / category;
}

struct NumericSuffix {
    std::string base;
    int value{0};
    bool has_suffix{false};
};

NumericSuffix split_numeric_suffix(const std::string& stem) {
    NumericSuffix result{stem, 0, false};
    const auto pos = stem.rfind('_');
    if (pos == std::string::npos || pos + 1 >= stem.size()) {
        return result;
    }
    const std::string suffix = stem.substr(pos + 1);
    if (suffix.empty()) {
        return result;
    }
    for (unsigned char ch : suffix) {
        if (!std::isdigit(ch)) {
            return result;
        }
    }
    int value = 0;
    try {
        value = std::stoi(suffix);
    } catch (...) {
        return result;
    }
    if (value <= 0) {
        return result;
    }
    const std::string base = stem.substr(0, pos);
    if (base.empty()) {
        return result;
    }
    result.base = base;
    result.value = value;
    result.has_suffix = true;
    return result;
}

struct ParentheticalSuffix {
    std::string base;
    int value{0};
    bool has_suffix{false};
};

ParentheticalSuffix split_parenthetical_suffix(const std::string& stem) {
    ParentheticalSuffix result{stem, 0, false};
    if (stem.size() < 4) {
        return result;
    }
    if (stem.back() != ')') {
        return result;
    }
    const auto open_pos = stem.rfind('(');
    if (open_pos == std::string::npos || open_pos == 0) {
        return result;
    }
    if (open_pos == 0 || stem[open_pos - 1] != ' ') {
        return result;
    }
    const std::string number = stem.substr(open_pos + 1, stem.size() - open_pos - 2);
    if (number.empty()) {
        return result;
    }
    for (unsigned char ch : number) {
        if (!std::isdigit(ch)) {
            return result;
        }
    }
    int value = 0;
    try {
        value = std::stoi(number);
    } catch (...) {
        return result;
    }
    if (value <= 0) {
        return result;
    }
    const std::string base = stem.substr(0, open_pos - 1);
    if (base.empty()) {
        return result;
    }
    result.base = base;
    result.value = value;
    result.has_suffix = true;
    return result;
}

std::string build_unique_suggested_name(const std::string& desired_name,
                                        const std::filesystem::path& target_dir,
                                        std::unordered_set<std::string>& used_names,
                                        std::unordered_map<std::string, int>& next_index,
                                        bool force_numbering)
{
    auto conflicts = [&](const std::string& candidate) -> bool {
        const std::string candidate_lower = to_lower_copy_str(candidate);
        if (used_names.count(candidate_lower) > 0) {
            return true;
        }
        if (!target_dir.empty()) {
            std::error_code ec;
            const auto candidate_path = target_dir / Utils::utf8_to_path(candidate);
            if (std::filesystem::exists(candidate_path, ec)) {
                return true;
            }
        }
        return false;
    };

    const auto desired_path = Utils::utf8_to_path(desired_name);
    std::string stem = Utils::path_to_utf8(desired_path.stem());
    std::string ext = Utils::path_to_utf8(desired_path.extension());
    if (stem.empty()) {
        stem = desired_name;
        ext.clear();
    }

    const NumericSuffix suffix = split_numeric_suffix(stem);
    std::string base_stem = stem;
    int index = 1;
    bool has_suffix = false;
    if (suffix.has_suffix) {
        base_stem = suffix.base;
        index = suffix.value;
        has_suffix = true;
    }

    if (!force_numbering && !conflicts(desired_name)) {
        return desired_name;
    }
    if (has_suffix && !conflicts(desired_name)) {
        return desired_name;
    }

    const std::string key = has_suffix
                                ? to_lower_copy_str(base_stem + ext)
                                : to_lower_copy_str(desired_name);
    auto it = next_index.find(key);
    if (it != next_index.end()) {
        index = it->second;
    }

    while (true) {
        std::string candidate = base_stem + "_" + std::to_string(index) + ext;
        if (!conflicts(candidate)) {
            next_index[key] = index + 1;
            return candidate;
        }
        ++index;
    }
}

std::string build_unique_move_name(const std::string& desired_name,
                                   const std::filesystem::path& target_dir,
                                   std::unordered_set<std::string>& used_names,
                                   std::unordered_map<std::string, int>& next_index)
{
    auto conflicts = [&](const std::string& candidate) -> bool {
        const std::string candidate_lower = to_lower_copy_str(candidate);
        if (used_names.count(candidate_lower) > 0) {
            return true;
        }
        if (!target_dir.empty()) {
            std::error_code ec;
            const auto candidate_path = target_dir / Utils::utf8_to_path(candidate);
            if (std::filesystem::exists(candidate_path, ec)) {
                return true;
            }
        }
        return false;
    };

    if (!conflicts(desired_name)) {
        return desired_name;
    }

    const auto desired_path = Utils::utf8_to_path(desired_name);
    std::string stem = Utils::path_to_utf8(desired_path.stem());
    std::string ext = Utils::path_to_utf8(desired_path.extension());
    if (stem.empty()) {
        stem = desired_name;
        ext.clear();
    }

    const ParentheticalSuffix suffix = split_parenthetical_suffix(stem);
    std::string base_stem = suffix.has_suffix ? suffix.base : stem;
    int index = suffix.has_suffix ? suffix.value : 1;

    const std::string key = to_lower_copy_str(base_stem + ext);
    auto it = next_index.find(key);
    if (it != next_index.end()) {
        index = std::max(index, it->second);
    }

    while (true) {
        std::string candidate = base_stem + " (" + std::to_string(index) + ")" + ext;
        if (!conflicts(candidate)) {
            next_index[key] = index + 1;
            return candidate;
        }
        ++index;
    }
}

void ensure_unique_image_suggested_names(std::vector<CategorizedFile>& files,
                                         const std::string& base_dir,
                                         bool use_subcategory)
{
    std::unordered_map<std::string, std::unordered_map<std::string, int>> counts;
    counts.reserve(files.size());

    for (const auto& file : files) {
        if (file.suggested_name.empty()) {
            continue;
        }
        if (file.rename_applied) {
            continue;
        }
        if (to_lower_copy_str(file.suggested_name) == to_lower_copy_str(file.file_name)) {
            continue;
        }
        if (!is_supported_image_entry(file.file_path, file.file_name, file.type)) {
            continue;
        }
        const auto target_dir = build_suggested_target_dir(file, base_dir, use_subcategory);
        const std::string dir_key = Utils::path_to_utf8(target_dir);
        const std::string name_key = to_lower_copy_str(file.suggested_name);
        counts[dir_key][name_key] += 1;
    }

    std::unordered_map<std::string, std::unordered_set<std::string>> used_names;
    std::unordered_map<std::string, std::unordered_map<std::string, int>> next_index;

    for (auto& file : files) {
        if (file.suggested_name.empty()) {
            continue;
        }
        if (file.rename_applied) {
            continue;
        }
        if (to_lower_copy_str(file.suggested_name) == to_lower_copy_str(file.file_name)) {
            continue;
        }
        if (!is_supported_image_entry(file.file_path, file.file_name, file.type)) {
            continue;
        }
        const auto target_dir = build_suggested_target_dir(file, base_dir, use_subcategory);
        const std::string dir_key = Utils::path_to_utf8(target_dir);
        const std::string name_key = to_lower_copy_str(file.suggested_name);
        const bool force_numbering = counts[dir_key][name_key] > 1;
        auto& dir_used = used_names[dir_key];
        auto& dir_next = next_index[dir_key];

        const std::string unique_name = build_unique_suggested_name(file.suggested_name,
                                                                    target_dir,
                                                                    dir_used,
                                                                    dir_next,
                                                                    force_numbering);
        file.suggested_name = unique_name;
        dir_used.insert(to_lower_copy_str(unique_name));
    }
}
}

void CategorizationDialog::ensure_unique_suggested_names_in_model()
{
    if (!model) {
        return;
    }

    struct RowEntry {
        int row{0};
        std::string file_path;
        std::string file_name;
        FileType type{FileType::File};
        std::string category;
        std::string subcategory;
        std::string suggested_name;
        bool rename_only{false};
        bool rename_applied{false};
    };

    std::vector<RowEntry> entries;
    entries.reserve(model->rowCount());

    for (int row = 0; row < model->rowCount(); ++row) {
        if (!row_is_supported_image(row)) {
            continue;
        }
        auto* file_item = model->item(row, ColumnFile);
        auto* rename_item = model->item(row, ColumnSuggestedName);
        if (!file_item || !rename_item) {
            continue;
        }
        const std::string suggested = rename_item->text().toStdString();
        if (suggested.empty()) {
            continue;
        }

        RowEntry entry;
        entry.row = row;
        entry.file_path = file_item->data(kFilePathRole).toString().toStdString();
        if (entry.file_path.empty()) {
            entry.file_path = base_dir_;
        }
        entry.file_name = file_item->text().toStdString();
        if (to_lower_copy_str(suggested) == to_lower_copy_str(entry.file_name)) {
            continue;
        }
        entry.type = static_cast<FileType>(file_item->data(kFileTypeRole).toInt());
        entry.rename_only = file_item->data(kRenameOnlyRole).toBool();
        entry.rename_applied = file_item->data(kRenameAppliedRole).toBool();
        if (entry.rename_applied) {
            continue;
        }
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
    }

    std::unordered_map<std::string, std::unordered_map<std::string, int>> counts;
    counts.reserve(entries.size());

    for (const auto& entry : entries) {
        CategorizedFile file;
        file.file_path = entry.file_path;
        file.file_name = entry.file_name;
        file.type = entry.type;
        file.category = entry.category;
        file.subcategory = entry.subcategory;
        file.rename_only = entry.rename_only;
        file.suggested_name = entry.suggested_name;
        file.rename_applied = entry.rename_applied;
        const auto target_dir = build_suggested_target_dir(file, base_dir_, show_subcategory_column);
        const std::string dir_key = Utils::path_to_utf8(target_dir);
        const std::string name_key = to_lower_copy_str(entry.suggested_name);
        counts[dir_key][name_key] += 1;
    }

    std::unordered_map<std::string, std::unordered_set<std::string>> used_names;
    std::unordered_map<std::string, std::unordered_map<std::string, int>> next_index;

    for (auto& entry : entries) {
        CategorizedFile file;
        file.file_path = entry.file_path;
        file.file_name = entry.file_name;
        file.type = entry.type;
        file.category = entry.category;
        file.subcategory = entry.subcategory;
        file.rename_only = entry.rename_only;
        file.suggested_name = entry.suggested_name;
        file.rename_applied = entry.rename_applied;
        const auto target_dir = build_suggested_target_dir(file, base_dir_, show_subcategory_column);
        const std::string dir_key = Utils::path_to_utf8(target_dir);
        const std::string name_key = to_lower_copy_str(entry.suggested_name);
        const bool force_numbering = counts[dir_key][name_key] > 1;
        auto& dir_used = used_names[dir_key];
        auto& dir_next = next_index[dir_key];

        const std::string unique_name = build_unique_suggested_name(entry.suggested_name,
                                                                    target_dir,
                                                                    dir_used,
                                                                    dir_next,
                                                                    force_numbering);
        dir_used.insert(to_lower_copy_str(unique_name));
        if (unique_name != entry.suggested_name) {
            if (auto* rename_item = model->item(entry.row, ColumnSuggestedName)) {
                rename_item->setText(QString::fromStdString(unique_name));
            }
            entry.suggested_name = unique_name;
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
        category_item->setEditable(!file.rename_only);
        if (!file.rename_only) {
            category_item->setIcon(edit_icon());
        }

        std::string subcategory_text = file.subcategory;
        if (is_image_entry && is_missing_category_label(subcategory_text)) {
            subcategory_text.clear();
        }
        auto* subcategory_item = new QStandardItem(QString::fromStdString(subcategory_text));
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


void CategorizationDialog::record_categorization_to_db()
{
    if (!db_manager) {
        return;
    }

    auto entry_is_unchanged = [](const CategorizedFile& cached,
                                 const std::string& category,
                                 const std::string& subcategory,
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
        if (cached.category != category || cached.subcategory != subcategory) {
            return false;
        }
        if (!resolved.category.empty() && cached.taxonomy_id != resolved.taxonomy_id) {
            return false;
        }
        return true;
    };

    for (int row = 0; row < model->rowCount(); ++row) {
        auto* file_item = model->item(row, ColumnFile);
        if (!file_item) {
            continue;
        }
        bool rename_only = file_item->data(kRenameOnlyRole).toBool();
        auto* category_item = model->item(row, ColumnCategory);
        auto* subcategory_item = model->item(row, ColumnSubcategory);
        std::string category = category_item ? category_item->text().toStdString() : std::string();
        std::string subcategory = show_subcategory_column && subcategory_item
                                      ? subcategory_item->text().toStdString()
                                      : std::string();
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
                category = cached_entry->category;
                subcategory = cached_entry->subcategory;
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
        }
        if (rename_only) {
            DatabaseManager::ResolvedCategory resolved{0, "", ""};
            if (cached_entry && !category.empty()) {
                resolved.taxonomy_id = cached_entry->taxonomy_id;
                resolved.category = category;
                resolved.subcategory = subcategory;
            }
            const std::string file_type_label = (file_type == FileType::Directory) ? "D" : "F";
            if (cached_entry &&
                entry_is_unchanged(*cached_entry,
                                   category,
                                   subcategory,
                                   resolved,
                                   suggested_name,
                                   rename_only,
                                   used_consistency)) {
                continue;
            }
            db_manager->insert_or_update_file_with_categorization(
                file_name, file_type_label, file_path, resolved, used_consistency, suggested_name, true);
            continue;
        }

        if (!category_item) {
            continue;
        }

        auto resolved = db_manager->resolve_category(category, subcategory);

        const std::string file_type_label = (file_type == FileType::Directory) ? "D" : "F";
        if (cached_entry &&
            entry_is_unchanged(*cached_entry,
                               category,
                               subcategory,
                               resolved,
                               suggested_name,
                               rename_only,
                               used_consistency)) {
            continue;
        }
        db_manager->insert_or_update_file_with_categorization(
            file_name, file_type_label, file_path, resolved, used_consistency, suggested_name);

        category_item->setText(QString::fromStdString(resolved.category));
        if (show_subcategory_column) {
            if (subcategory_item) {
                subcategory_item->setText(QString::fromStdString(resolved.subcategory));
            }
        }
    }
}


void CategorizationDialog::on_confirm_and_sort_button_clicked()
{
    record_categorization_to_db();

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

    const bool dry_run = dry_run_checkbox && dry_run_checkbox->isChecked();
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

            std::string category = category_item->text().toStdString();
            if (is_missing_category_label(category)) {
                continue;
            }
            std::string subcategory = show_subcategory_column && subcategory_item
                ? subcategory_item->text().toStdString()
                : std::string();
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
        const std::string category = category_item->text().toStdString();
        const std::string subcategory = show_subcategory_column && subcategory_item
                                            ? subcategory_item->text().toStdString()
                                            : std::string();
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

    if (files_not_moved.empty()) {
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

        if (!std::filesystem::exists(source_path)) {
            update_status_column(row_index, false);
            files_not_moved.push_back(file_name);
            if (core_logger) {
                core_logger->warn("Source file missing when renaming '{}'", file_name);
            }
            return;
        }
        if (std::filesystem::exists(dest_path)) {
            update_status_column(row_index, false);
            files_not_moved.push_back(file_name);
            if (core_logger) {
                core_logger->warn("Destination already exists for rename '{}'", destination_name);
            }
            return;
        }

        try {
            std::filesystem::rename(source_path, dest_path);
            update_status_column(row_index, true, true, rename_active, false);

            std::error_code ec;
            const std::uintmax_t size_bytes = std::filesystem::file_size(dest_path, ec);
            std::time_t mtime_value = 0;
            if (!ec) {
                const auto ftime = std::filesystem::last_write_time(dest_path, ec);
                if (!ec) {
                    const auto sys = to_system_clock(ftime);
                    mtime_value = std::chrono::system_clock::to_time_t(sys);
                }
            }
            record_move_for_undo(row_index,
                                 Utils::path_to_utf8(source_path),
                                 Utils::path_to_utf8(dest_path),
                                 size_bytes,
                                 mtime_value);
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
        } catch (const std::exception& ex) {
            update_status_column(row_index, false);
            files_not_moved.push_back(file_name);
            if (core_logger) {
                core_logger->error("Failed to rename '{}': {}", file_name, ex.what());
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
            source_dir, base_dir, category, effective_subcategory, file_name, destination_name);

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
        bool moved = categorized_file.move_file(show_subcategory_column);
        update_status_column(row_index, moved, true, rename_active && moved, moved);

        if (!moved) {
            files_not_moved.push_back(file_name);
            if (core_logger) {
                core_logger->warn("File {} already exists in the destination.", file_name);
            }
        } else {
            std::error_code ec;
            const std::uintmax_t size_bytes = std::filesystem::file_size(Utils::utf8_to_path(preview_paths.destination), ec);
            std::time_t mtime_value = 0;
            if (!ec) {
                const auto ftime = std::filesystem::last_write_time(Utils::utf8_to_path(preview_paths.destination), ec);
                if (!ec) {
                    const auto sys = to_system_clock(ftime);
                    mtime_value = std::chrono::system_clock::to_time_t(sys);
                }
            }
            record_move_for_undo(row_index, preview_paths.source, preview_paths.destination, size_bytes, mtime_value);

            if (db_manager && (rename_active || include_subdirectories_)) {
                auto resolved = db_manager->resolve_category(category, effective_subcategory);
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
        if (!attempted) {
            status = RowStatus::NotSelected;
            status_item->setForeground(QBrush(Qt::gray));
        } else if (success) {
            if (renamed && moved) {
                status = RowStatus::RenamedAndMoved;
            } else if (renamed) {
                status = RowStatus::Renamed;
            } else {
                status = RowStatus::Moved;
            }
            status_item->setForeground(QBrush(Qt::darkGreen));
        } else {
            status = RowStatus::Skipped;
            status_item->setForeground(QBrush(Qt::red));
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
                                                std::time_t mtime)
{
    move_history_.push_back(MoveRecord{row, source, destination, size_bytes, mtime});
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
    std::error_code ec;
    auto destination_path = Utils::utf8_to_path(destination);
    auto source_path = Utils::utf8_to_path(source);

    if (!std::filesystem::exists(destination_path)) {
        if (core_logger) {
            core_logger->warn("Undo skipped; destination '{}' missing", destination);
        }
        return false;
    }

    std::filesystem::create_directories(source_path.parent_path(), ec);

    try {
        std::filesystem::rename(destination_path, source_path);
    } catch (const std::filesystem::filesystem_error& ex) {
        if (core_logger) {
            core_logger->error("Undo move failed '{}' -> '{}': {}", destination, source, ex.what());
        }
        return false;
    }

    remove_empty_parent_directories(destination);
    return true;
}

bool CategorizationDialog::undo_move_history()
{
    if (move_history_.empty()) {
    return false;
    }

    bool any_success = false;
    for (auto it = move_history_.rbegin(); it != move_history_.rend(); ++it) {
        if (move_file_back(it->source_path, it->destination_path)) {
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

    const std::string category = category_item->text().toStdString();
    const std::string subcategory = show_subcategory_column && subcategory_item
        ? subcategory_item->text().toStdString()
        : std::string();
    const std::string effective_subcategory = subcategory.empty() ? category : subcategory;

    std::string validation_error;
    const bool allow_identical = !show_subcategory_column;
    if (!validate_labels(category, effective_subcategory, validation_error, allow_identical)) {
        return fail("Validation failed: " + validation_error);
    }

    try {
        MovableCategorizedFile categorized_file(source_dir,
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
    if (name.empty()) {
        error = "Filename is empty";
        return false;
    }
    if (name == "." || name == "..") {
        error = "Filename is invalid";
        return false;
    }
    if (!contains_only_allowed_chars(name)) {
        error = "Filename contains disallowed characters";
        return false;
    }
    if (has_leading_or_trailing_space_or_dot(name)) {
        error = "Filename has leading/trailing space or dot";
        return false;
    }

    const auto path = Utils::utf8_to_path(name);
    const std::string stem = Utils::path_to_utf8(path.stem());
    if (stem.empty()) {
        error = "Filename is missing a base name";
        return false;
    }
    if (is_reserved_windows_name(stem)) {
        error = "Filename is a reserved name";
        return false;
    }
    return true;
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
        status_item->setForeground(QBrush(Qt::blue));
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
            rec.mtime});
    }

    UndoManager manager(undo_dir_);
    manager.save_plan(base_dir_, entries, core_logger);
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
    set_text_if(select_highlighted_button, tr("Select highlighted"));
    set_text_if(bulk_edit_button, tr("Edit selected..."));
    set_text_if(show_subcategories_checkbox, tr("Create subcategory folders"));
    set_text_if(dry_run_checkbox, tr("Dry run (preview only, do not move files)"));
    set_text_if(rename_images_only_checkbox, tr("Do not categorize picture files (only rename)"));
    set_text_if(rename_documents_only_checkbox, tr("Do not categorize document files (only rename)"));
    set_text_if(confirm_button, tr("Confirm and Process"));
    set_text_if(continue_button, tr("Continue Later"));
    set_text_if(undo_button, tr("Undo this change"));
    set_text_if(close_button, tr("Close"));

    if (select_highlighted_button) {
        select_highlighted_button->setToolTip(tr("Mark highlighted rows for processing (Ctrl+Space)."));
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
            tr("Planned destination")
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
    if (event && event->type() == QEvent::LanguageChange) {
        retranslate_ui();
        for (int row = 0; row < model->rowCount(); ++row) {
            update_preview_column(row);
        }
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
    ensure_unique_image_suggested_names(categorized_files, base_dir_, show_subcategory_column);
    set_show_rename_column(std::any_of(categorized_files.begin(),
                                       categorized_files.end(),
                                       [](const CategorizedFile& file) {
                                           return !file.suggested_name.empty();
                                       }));
    update_rename_only_checkbox_state();
    apply_category_visibility();
    apply_subcategory_visibility();
    populate_model();
    on_rename_images_only_toggled(rename_images_only_checkbox && rename_images_only_checkbox->isChecked());
    on_rename_documents_only_toggled(rename_documents_only_checkbox && rename_documents_only_checkbox->isChecked());
    update_subcategory_checkbox_state();
}

void CategorizationDialog::test_trigger_confirm() {
    on_confirm_and_sort_button_clicked();
}

void CategorizationDialog::test_trigger_undo() {
    on_undo_button_clicked();
}

bool CategorizationDialog::test_undo_enabled() const {
    return undo_button && undo_button->isEnabled();
}
#endif
