#include "FolderStructureInitializerDialog.hpp"

#include <QAbstractItemView>
#include <QByteArray>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QVBoxLayout>
#include <filesystem>
#include <string>

#include "Utils.hpp"

namespace {

QString from_utf8(const std::string& value) {
    return QString::fromUtf8(value.c_str(), static_cast<int>(value.size()));
}

std::string to_utf8(const QString& value) {
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

QString path_to_qstring(const std::filesystem::path& path) {
    return from_utf8(Utils::path_to_utf8(path));
}

}  // namespace

FolderStructureInitializerDialog::FolderStructureInitializerDialog(const std::filesystem::path& start_directory,
                                                                   QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Create folder structure"));
    setMinimumSize(760, 520);

    auto* root_layout = new QVBoxLayout(this);

    auto* content_layout = new QHBoxLayout();
    root_layout->addLayout(content_layout, 1);

    template_list_ = new QListWidget(this);
    template_list_->setSelectionMode(QAbstractItemView::SingleSelection);
    template_list_->setMinimumWidth(260);
    template_list_->setObjectName(QStringLiteral("folderStructureTemplateList"));
    content_layout->addWidget(template_list_);

    auto* detail_layout = new QVBoxLayout();
    content_layout->addLayout(detail_layout, 1);

    description_label_ = new QLabel(this);
    description_label_->setWordWrap(true);
    description_label_->setObjectName(QStringLiteral("folderStructureDescriptionLabel"));
    detail_layout->addWidget(description_label_);

    auto* destination_row = new QHBoxLayout();
    destination_edit_ = new QLineEdit(this);
    destination_edit_->setObjectName(QStringLiteral("folderStructureDestinationEdit"));
    destination_edit_->setText(path_to_qstring(start_directory));
    browse_button_ = new QPushButton(tr("Browse..."), this);
    browse_button_->setObjectName(QStringLiteral("folderStructureBrowseButton"));
    destination_row->addWidget(destination_edit_, 1);
    destination_row->addWidget(browse_button_);

    auto* form_layout = new QFormLayout();
    form_layout->addRow(tr("Destination:"), destination_row);
    detail_layout->addLayout(form_layout);

    auto* preview_label = new QLabel(tr("Folders to create:"), this);
    detail_layout->addWidget(preview_label);

    preview_list_ = new QListWidget(this);
    preview_list_->setObjectName(QStringLiteral("folderStructurePreviewList"));
    preview_list_->setAlternatingRowColors(true);
    detail_layout->addWidget(preview_list_, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    create_button_ = buttons->addButton(tr("Create"), QDialogButtonBox::AcceptRole);
    create_button_->setObjectName(QStringLiteral("folderStructureCreateButton"));
    root_layout->addWidget(buttons);

    connect(template_list_, &QListWidget::currentItemChanged, this, [this]() { update_selection(); });
    connect(destination_edit_, &QLineEdit::textChanged, this, [this]() { update_selection(); });
    connect(browse_button_, &QPushButton::clicked, this, [this]() { browse_destination(); });
    connect(create_button_, &QPushButton::clicked, this, [this]() { create_selected_structure(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    populate_templates();
    update_selection();
}

std::filesystem::path FolderStructureInitializerDialog::destination_root() const {
    return Utils::utf8_to_path(to_utf8(destination_edit_ ? destination_edit_->text().trimmed() : QString()));
}

void FolderStructureInitializerDialog::populate_templates() {
    if (!template_list_) {
        return;
    }

    template_list_->clear();
    QListWidgetItem* first_available_item = nullptr;
    for (const auto& descriptor : FolderStructureTemplates::all()) {
        QString label = from_utf8(descriptor.name);
        if (!descriptor.available) {
            label += tr(" (coming later)");
        }

        auto* item = new QListWidgetItem(label, template_list_);
        item->setData(Qt::UserRole, static_cast<int>(descriptor.id));
        item->setToolTip(from_utf8(descriptor.description));
        if (!descriptor.available) {
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            item->setForeground(palette().brush(QPalette::Disabled, QPalette::Text));
        } else if (!first_available_item) {
            first_available_item = item;
        }
    }

    if (first_available_item) {
        template_list_->setCurrentItem(first_available_item);
    }
}

void FolderStructureInitializerDialog::update_selection() {
    const auto* descriptor = selected_template();
    const bool available = descriptor && descriptor->available;
    const bool has_destination = destination_edit_ && !destination_edit_->text().trimmed().isEmpty();

    if (description_label_) {
        description_label_->setText(descriptor ? from_utf8(descriptor->description) : QString());
    }
    if (preview_list_) {
        preview_list_->clear();
        if (descriptor) {
            for (const std::string& relative : descriptor->relative_directories) {
                preview_list_->addItem(from_utf8(relative));
            }
        }
    }
    if (create_button_) {
        create_button_->setEnabled(available && has_destination);
    }
}

void FolderStructureInitializerDialog::browse_destination() {
    const QString start_directory = destination_edit_ && !destination_edit_->text().trimmed().isEmpty()
                                        ? destination_edit_->text().trimmed()
                                        : QDir::homePath();
    const QString directory = QFileDialog::getExistingDirectory(this, tr("Choose destination folder"), start_directory);
    if (!directory.isEmpty() && destination_edit_) {
        destination_edit_->setText(directory);
    }
}

void FolderStructureInitializerDialog::create_selected_structure() {
    const auto* descriptor = selected_template();
    if (!descriptor || !descriptor->available) {
        QMessageBox::warning(this, tr("Create folder structure"), tr("Choose an available folder structure."));
        return;
    }

    const std::filesystem::path root = destination_root();
    const auto result = FolderStructureTemplates::create(root, descriptor->id);
    if (!result.success) {
        QMessageBox::warning(this, tr("Create folder structure"), from_utf8(result.error));
        return;
    }

    created_count_ = result.created_directories.size();
    existing_count_ = result.existing_directories.size();
    QMessageBox::information(this, tr("Folder structure created"),
                             tr("Created %1 folders. %2 folders already existed.")
                                 .arg(static_cast<qulonglong>(created_count_))
                                 .arg(static_cast<qulonglong>(existing_count_)));
    accept();
}

const FolderStructureTemplates::Descriptor* FolderStructureInitializerDialog::selected_template() const {
    if (!template_list_ || !template_list_->currentItem()) {
        return nullptr;
    }

    bool ok = false;
    const int value = template_list_->currentItem()->data(Qt::UserRole).toInt(&ok);
    if (!ok) {
        return nullptr;
    }
    return FolderStructureTemplates::find(static_cast<FolderStructureTemplates::Id>(value));
}
