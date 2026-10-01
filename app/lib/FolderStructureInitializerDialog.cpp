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
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <filesystem>
#include <string>

#include "JohnnyDecimalFolderSuggester.hpp"
#include "JohnnyDecimalArchiveIndex.hpp"
#include "JohnnyDecimalValidator.hpp"
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

FolderStructureInitializerDialog::FolderStructureInitializerDialog(
    const std::filesystem::path& start_directory, const std::vector<FolderStructurePluginProfile>& plugin_profiles,
    QWidget* parent)
    : QDialog(parent),
      plugin_profiles_(plugin_profiles),
      descriptors_(FolderStructureTemplates::all_with_plugins(plugin_profiles)) {
    setWindowTitle(tr("Create folder structure"));
    setMinimumSize(760, 520);

    auto* root_layout = new QVBoxLayout(this);

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
    root_layout->addLayout(form_layout);

    tab_widget_ = new QTabWidget(this);
    tab_widget_->setObjectName(QStringLiteral("folderStructureTabWidget"));
    root_layout->addWidget(tab_widget_, 1);

    starter_tab_ = new QWidget(tab_widget_);
    auto* starter_layout = new QHBoxLayout(starter_tab_);

    template_list_ = new QListWidget(this);
    template_list_->setSelectionMode(QAbstractItemView::SingleSelection);
    template_list_->setMinimumWidth(260);
    template_list_->setObjectName(QStringLiteral("folderStructureTemplateList"));
    starter_layout->addWidget(template_list_);

    auto* detail_layout = new QVBoxLayout();
    starter_layout->addLayout(detail_layout, 1);

    description_label_ = new QLabel(this);
    description_label_->setWordWrap(true);
    description_label_->setObjectName(QStringLiteral("folderStructureDescriptionLabel"));
    detail_layout->addWidget(description_label_);

    auto* preview_label = new QLabel(tr("Folders to create:"), this);
    detail_layout->addWidget(preview_label);

    preview_list_ = new QListWidget(this);
    preview_list_->setObjectName(QStringLiteral("folderStructurePreviewList"));
    preview_list_->setAlternatingRowColors(true);
    detail_layout->addWidget(preview_list_, 1);
    tab_widget_->addTab(starter_tab_, tr("Starter structure"));

    next_folder_tab_ = new QWidget(tab_widget_);
    auto* next_layout = new QVBoxLayout(next_folder_tab_);

    auto* next_form_layout = new QFormLayout();
    next_area_edit_ = new QLineEdit(this);
    next_area_edit_->setObjectName(QStringLiteral("johnnyDecimalNextAreaEdit"));
    next_area_edit_->setPlaceholderText(QStringLiteral("Work"));
    next_folder_edit_ = new QLineEdit(this);
    next_folder_edit_->setObjectName(QStringLiteral("johnnyDecimalNextFolderEdit"));
    next_folder_edit_->setPlaceholderText(QStringLiteral("Proposals"));
    next_preview_edit_ = new QLineEdit(this);
    next_preview_edit_->setObjectName(QStringLiteral("johnnyDecimalNextPreviewEdit"));
    next_preview_edit_->setReadOnly(true);
    next_form_layout->addRow(tr("Area:"), next_area_edit_);
    next_form_layout->addRow(tr("Folder:"), next_folder_edit_);
    next_form_layout->addRow(tr("Preview:"), next_preview_edit_);
    next_layout->addLayout(next_form_layout);

    next_preview_status_label_ = new QLabel(this);
    next_preview_status_label_->setWordWrap(true);
    next_preview_status_label_->setObjectName(QStringLiteral("johnnyDecimalNextPreviewStatusLabel"));
    next_layout->addWidget(next_preview_status_label_);
    next_layout->addStretch(1);
    tab_widget_->addTab(next_folder_tab_, tr("Next folder"));

    archive_index_tab_ = new QWidget(tab_widget_);
    auto* archive_index_layout = new QVBoxLayout(archive_index_tab_);
    auto* archive_index_description = new QLabel(
        tr("View a readable map of Johnny.Decimal areas, categories, and item folders in the selected archive."),
        this);
    archive_index_description->setWordWrap(true);
    archive_index_layout->addWidget(archive_index_description);
    archive_index_edit_ = new QPlainTextEdit(this);
    archive_index_edit_->setObjectName(QStringLiteral("johnnyDecimalArchiveIndexEdit"));
    archive_index_edit_->setReadOnly(true);
    archive_index_edit_->setLineWrapMode(QPlainTextEdit::NoWrap);
    archive_index_layout->addWidget(archive_index_edit_, 1);
    tab_widget_->addTab(archive_index_tab_, tr("Archive index"));

    validation_tab_ = new QWidget(tab_widget_);
    auto* validation_layout = new QVBoxLayout(validation_tab_);
    auto* validation_description = new QLabel(
        tr("Check an existing Johnny.Decimal archive for duplicate IDs, malformed numbers, folders outside ranges, "
           "and missing area/category structure."),
        this);
    validation_description->setWordWrap(true);
    validation_layout->addWidget(validation_description);
    validation_report_edit_ = new QPlainTextEdit(this);
    validation_report_edit_->setObjectName(QStringLiteral("johnnyDecimalValidationReportEdit"));
    validation_report_edit_->setReadOnly(true);
    validation_report_edit_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    validation_layout->addWidget(validation_report_edit_, 1);
    tab_widget_->addTab(validation_tab_, tr("Validation report"));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    create_button_ = buttons->addButton(tr("Create"), QDialogButtonBox::AcceptRole);
    create_button_->setObjectName(QStringLiteral("folderStructureCreateButton"));
    root_layout->addWidget(buttons);

    connect(template_list_, &QListWidget::currentItemChanged, this, [this]() { update_selection(); });
    connect(destination_edit_, &QLineEdit::textChanged, this, [this]() { update_selection(); });
    connect(next_area_edit_, &QLineEdit::textChanged, this, [this]() { update_selection(); });
    connect(next_folder_edit_, &QLineEdit::textChanged, this, [this]() { update_selection(); });
    connect(browse_button_, &QPushButton::clicked, this, [this]() { browse_destination(); });
    connect(tab_widget_, &QTabWidget::currentChanged, this, [this]() { update_selection(); });
    connect(create_button_, &QPushButton::clicked, this, [this]() {
        if (archive_index_tab_active()) {
            refresh_archive_index();
        } else if (validation_tab_active()) {
            refresh_validation_report();
        } else if (next_folder_tab_active()) {
            create_next_johnny_decimal_folder();
        } else {
            create_selected_structure();
        }
    });
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
    for (std::size_t index = 0; index < descriptors_.size(); ++index) {
        const auto& descriptor = descriptors_[index];
        QString label = from_utf8(descriptor.name);
        if (!descriptor.plugin_profile_id.empty()) {
            label += tr(" (plugin)");
        }
        if (!descriptor.available) {
            label += tr(" (coming later)");
        }

        auto* item = new QListWidgetItem(label, template_list_);
        item->setData(Qt::UserRole, static_cast<int>(index));
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

    if (create_button_) {
        create_button_->setText(archive_index_tab_active()
                                    ? tr("Refresh index")
                                    : (validation_tab_active()
                                           ? tr("Refresh report")
                                           : (next_folder_tab_active() ? tr("Create folder") : tr("Create"))));
    }

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
    if (next_folder_tab_active()) {
        QString status;
        QString preview;
        bool can_create_next_folder = false;
        if (!has_destination) {
            status = tr("Choose a destination folder.");
        } else if (!next_area_edit_ || next_area_edit_->text().trimmed().isEmpty()) {
            status = tr("Enter an area name.");
        } else if (!next_folder_edit_ || next_folder_edit_->text().trimmed().isEmpty()) {
            status = tr("Enter a folder name.");
        } else {
            std::string error;
            const auto suggestion = JohnnyDecimalFolderSuggester::preview_next_folder(
                destination_root(), to_utf8(next_area_edit_->text().trimmed()),
                to_utf8(next_folder_edit_->text().trimmed()), plugin_profiles_, &error);
            if (suggestion) {
                preview = from_utf8(suggestion->relative_path);
                status = tr("Ready to create.");
                can_create_next_folder = true;
            } else {
                status = from_utf8(error);
            }
        }
        if (next_preview_edit_) {
            next_preview_edit_->setText(preview);
        }
        if (next_preview_status_label_) {
            next_preview_status_label_->setText(status);
        }
        if (create_button_) {
            create_button_->setEnabled(can_create_next_folder);
        }
        return;
    }
    if (archive_index_tab_active()) {
        refresh_archive_index();
        if (create_button_) {
            create_button_->setEnabled(has_destination);
        }
        return;
    }
    if (validation_tab_active()) {
        refresh_validation_report();
        if (create_button_) {
            create_button_->setEnabled(has_destination);
        }
        return;
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
    const auto result = FolderStructureTemplates::create(root, *descriptor);
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

void FolderStructureInitializerDialog::create_next_johnny_decimal_folder() {
    if (!next_area_edit_ || !next_folder_edit_) {
        return;
    }

    const auto result = JohnnyDecimalFolderSuggester::create_next_folder(
        destination_root(), to_utf8(next_area_edit_->text().trimmed()), to_utf8(next_folder_edit_->text().trimmed()),
        plugin_profiles_);
    if (!result.success) {
        QMessageBox::warning(this, tr("Create Johnny.Decimal folder"), from_utf8(result.error));
        update_selection();
        return;
    }

    created_count_ = result.created_directories.size();
    existing_count_ = 0;
    QMessageBox::information(this, tr("Johnny.Decimal folder created"),
                             tr("Created %1.").arg(from_utf8(result.suggestion.relative_path)));
    accept();
}

void FolderStructureInitializerDialog::refresh_archive_index() {
    if (!archive_index_edit_) {
        return;
    }
    if (!destination_edit_ || destination_edit_->text().trimmed().isEmpty()) {
        archive_index_edit_->setPlainText(tr("Choose a destination folder."));
        return;
    }

    const auto index = JohnnyDecimalArchiveIndex::build_index(destination_root());
    archive_index_edit_->setPlainText(from_utf8(JohnnyDecimalArchiveIndex::format_index(index)));
}

void FolderStructureInitializerDialog::refresh_validation_report() {
    if (!validation_report_edit_) {
        return;
    }
    if (!destination_edit_ || destination_edit_->text().trimmed().isEmpty()) {
        validation_report_edit_->setPlainText(tr("Choose a destination folder."));
        return;
    }

    const auto report = JohnnyDecimalValidator::validate_archive(destination_root());
    validation_report_edit_->setPlainText(from_utf8(JohnnyDecimalValidator::format_report(report)));
}

bool FolderStructureInitializerDialog::next_folder_tab_active() const {
    return tab_widget_ && next_folder_tab_ && tab_widget_->currentWidget() == next_folder_tab_;
}

bool FolderStructureInitializerDialog::archive_index_tab_active() const {
    return tab_widget_ && archive_index_tab_ && tab_widget_->currentWidget() == archive_index_tab_;
}

bool FolderStructureInitializerDialog::validation_tab_active() const {
    return tab_widget_ && validation_tab_ && tab_widget_->currentWidget() == validation_tab_;
}

const FolderStructureTemplates::Descriptor* FolderStructureInitializerDialog::selected_template() const {
    if (!template_list_ || !template_list_->currentItem()) {
        return nullptr;
    }

    bool ok = false;
    const int value = template_list_->currentItem()->data(Qt::UserRole).toInt(&ok);
    if (!ok || value < 0 || static_cast<std::size_t>(value) >= descriptors_.size()) {
        return nullptr;
    }
    return &descriptors_[static_cast<std::size_t>(value)];
}
