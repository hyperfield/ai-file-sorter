#include "Utils.hpp"
#include "CacheMaintenanceDialog.hpp"

#include "CacheMaintenanceService.hpp"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace {

QString dialog_tr(const char* source)
{
    return QCoreApplication::translate("CacheMaintenanceDialog", source);
}

QString path_display_text(const CacheMaintenanceTargetInfo& info)
{
    if (info.path.empty()) {
        return dialog_tr("None");
    }
    return QString::fromStdString(Utils::path_to_utf8(info.path));
}

QString size_display_text(const CacheMaintenanceTargetInfo& info)
{
    if (!info.exists) {
        return dialog_tr("None");
    }
    return QString::fromStdString(CacheMaintenanceService::format_size(info.size_bytes));
}

QString target_button_object_name(CacheMaintenanceTarget target)
{
    switch (target) {
    case CacheMaintenanceTarget::Categorization:
        return QStringLiteral("categorizationClearButton");
    case CacheMaintenanceTarget::ImageLocation:
        return QStringLiteral("imageLocationClearButton");
    case CacheMaintenanceTarget::Logs:
        return QStringLiteral("logsClearButton");
    }
    return QStringLiteral("clearButton");
}

QString target_panel_object_name(CacheMaintenanceTarget target)
{
    switch (target) {
    case CacheMaintenanceTarget::Categorization:
        return QStringLiteral("categorizationCachePanel");
    case CacheMaintenanceTarget::ImageLocation:
        return QStringLiteral("imageLocationCachePanel");
    case CacheMaintenanceTarget::Logs:
        return QStringLiteral("logsCachePanel");
    }
    return QStringLiteral("cachePanel");
}

QString target_title_object_name(CacheMaintenanceTarget target)
{
    switch (target) {
    case CacheMaintenanceTarget::Categorization:
        return QStringLiteral("categorizationTitleLabel");
    case CacheMaintenanceTarget::ImageLocation:
        return QStringLiteral("imageLocationTitleLabel");
    case CacheMaintenanceTarget::Logs:
        return QStringLiteral("logsTitleLabel");
    }
    return QStringLiteral("titleLabel");
}

QString target_description_object_name(CacheMaintenanceTarget target)
{
    switch (target) {
    case CacheMaintenanceTarget::Categorization:
        return QStringLiteral("categorizationDescriptionLabel");
    case CacheMaintenanceTarget::ImageLocation:
        return QStringLiteral("imageLocationDescriptionLabel");
    case CacheMaintenanceTarget::Logs:
        return QStringLiteral("logsDescriptionLabel");
    }
    return QStringLiteral("descriptionLabel");
}

QString target_path_value_object_name(CacheMaintenanceTarget target)
{
    switch (target) {
    case CacheMaintenanceTarget::Categorization:
        return QStringLiteral("categorizationPathValueLabel");
    case CacheMaintenanceTarget::ImageLocation:
        return QStringLiteral("imageLocationPathValueLabel");
    case CacheMaintenanceTarget::Logs:
        return QStringLiteral("logsPathValueLabel");
    }
    return QStringLiteral("pathValueLabel");
}

} // namespace

CacheMaintenanceDialog::CacheMaintenanceDialog(CacheMaintenanceService& service,
                                               bool busy,
                                               QWidget* parent)
    : QDialog(parent),
      service_(service),
      busy_(busy)
{
    setWindowTitle(dialog_tr("Clear cache"));
    setMinimumWidth(680);
    resize(760, 520);

    auto* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(12, 12, 12, 12);
    root_layout->setSpacing(12);

    auto* intro = new QLabel(
        dialog_tr("Remove cached categorization results, image place lookups, or log files. "
                  "Downloaded models are managed separately in the LLM dialog."),
        this);
    intro->setWordWrap(true);
    intro->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    root_layout->addWidget(intro);

    if (busy_) {
        busy_label_ = new QLabel(
            dialog_tr("Cache cleanup is unavailable while analysis is running."),
            this);
        busy_label_->setObjectName(QStringLiteral("cacheBusyLabel"));
        busy_label_->setWordWrap(true);
        busy_label_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
        root_layout->addWidget(busy_label_);
    }

    content_widget_ = new QWidget(this);
    content_layout_ = new QVBoxLayout(content_widget_);
    content_layout_->setSpacing(12);
    content_layout_->setContentsMargins(0, 0, 0, 0);

    content_scroll_area_ = new QScrollArea(this);
    content_scroll_area_->setWidgetResizable(true);
    content_scroll_area_->setFrameShape(QFrame::NoFrame);
    content_scroll_area_->setWidget(content_widget_);
    root_layout->addWidget(content_scroll_area_, 1);

    add_target_section(CacheMaintenanceTarget::Categorization);
    add_target_section(CacheMaintenanceTarget::ImageLocation);
    add_target_section(CacheMaintenanceTarget::Logs);

    auto* button_box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::accept);
    root_layout->addWidget(button_box);
}

void CacheMaintenanceDialog::add_target_section(CacheMaintenanceTarget target)
{
    auto* panel = new QFrame(content_widget_);
    panel->setObjectName(target_panel_object_name(target));
    panel->setFrameShape(QFrame::StyledPanel);
    panel->setFrameShadow(QFrame::Raised);
    panel->setToolTip(target_description(target));

    auto* layout = new QVBoxLayout(panel);
    layout->setSpacing(6);
    layout->setContentsMargins(12, 12, 12, 12);

    auto* title_label = new QLabel(target_title(target), panel);
    title_label->setObjectName(target_title_object_name(target));
    QFont title_font = title_label->font();
    title_font.setBold(true);
    title_label->setFont(title_font);
    title_label->setWordWrap(true);
    title_label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    title_label->setToolTip(target_description(target));
    layout->addWidget(title_label);

    auto* description = new QLabel(target_description(target), panel);
    description->setObjectName(target_description_object_name(target));
    description->setWordWrap(true);
    description->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    description->setToolTip(target_description(target));
    layout->addWidget(description);

    auto* path_caption = new QLabel(dialog_tr("Path:"), panel);
    path_caption->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    layout->addWidget(path_caption);

    auto* path_value = new QLabel(panel);
    path_value->setObjectName(target_path_value_object_name(target));
    path_value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    path_value->setWordWrap(true);
    path_value->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    path_value->setToolTip(target_description(target));
    layout->addWidget(path_value);

    auto* size_caption = new QLabel(dialog_tr("Estimated size:"), panel);
    size_caption->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    layout->addWidget(size_caption);

    auto* size_value = new QLabel(panel);
    size_value->setWordWrap(true);
    size_value->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    size_value->setToolTip(target_description(target));
    layout->addWidget(size_value);

    auto* button_row = new QHBoxLayout();
    button_row->addStretch(1);

    auto* clear_button = new QPushButton(dialog_tr("Clear"), panel);
    clear_button->setObjectName(target_button_object_name(target));
    clear_button->setToolTip(target_description(target));
    clear_button->setEnabled(!busy_);
    clear_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    button_row->addWidget(clear_button);
    layout->addLayout(button_row);

    connect(clear_button, &QPushButton::clicked, this, [this, target]() {
        clear_target(target);
    });

    target_widgets_[target] = TargetWidgets{
        .title_label = title_label,
        .description_label = description,
        .path_value = path_value,
        .size_value = size_value,
        .clear_button = clear_button
    };
    content_layout_->addWidget(panel);
    refresh_target(target);
}

void CacheMaintenanceDialog::refresh_target(CacheMaintenanceTarget target)
{
    const auto iter = target_widgets_.find(target);
    if (iter == target_widgets_.end()) {
        return;
    }

    const CacheMaintenanceTargetInfo info = service_.target_info(target);
    iter->second.path_value->setText(path_display_text(info));
    iter->second.size_value->setText(size_display_text(info));
    iter->second.clear_button->setEnabled(!busy_ && !info.path.empty());
}

void CacheMaintenanceDialog::clear_target(CacheMaintenanceTarget target)
{
    const CacheMaintenanceTargetInfo info = service_.target_info(target);
    if (!info.exists) {
        QMessageBox::information(this,
                                 windowTitle(),
                                 dialog_tr("This cache is already empty."));
        refresh_target(target);
        return;
    }

    QMessageBox confirm(this);
    confirm.setIcon(QMessageBox::Question);
    confirm.setWindowTitle(dialog_tr("Confirm cache clear"));
    confirm.setText(dialog_tr("Clear %1?").arg(target_title(target)));
    confirm.setInformativeText(
        dialog_tr("Path: %1\nEstimated reclaimed space: %2\n\nThis cannot be undone.")
            .arg(path_display_text(info),
                 size_display_text(info)));
    auto* clear_button = confirm.addButton(dialog_tr("Clear"), QMessageBox::AcceptRole);
    confirm.addButton(QMessageBox::Cancel);
    confirm.setDefaultButton(static_cast<QPushButton*>(nullptr));
    confirm.exec();
    if (confirm.clickedButton() != clear_button) {
        return;
    }

    std::string error;
    if (!service_.clear(target, &error)) {
        QMessageBox::warning(
            this,
            windowTitle(),
            error.empty() ? dialog_tr("The selected cache could not be removed.")
                          : QString::fromStdString(error));
        refresh_target(target);
        return;
    }

    refresh_target(target);
    QMessageBox::information(this,
                             windowTitle(),
                             dialog_tr("Cleared %1.").arg(target_title(target)));
}

QString CacheMaintenanceDialog::target_title(CacheMaintenanceTarget target) const
{
    switch (target) {
    case CacheMaintenanceTarget::Categorization:
        return dialog_tr("Categorization cache");
    case CacheMaintenanceTarget::ImageLocation:
        return dialog_tr("Image location cache");
    case CacheMaintenanceTarget::Logs:
        return dialog_tr("Logs");
    }
    return QString();
}

QString CacheMaintenanceDialog::target_description(CacheMaintenanceTarget target) const
{
    switch (target) {
    case CacheMaintenanceTarget::Categorization:
        return QCoreApplication::translate(
            "CacheMaintenanceDialog",
            "Saves earlier sorting suggestions so files and folders can be analyzed faster next time.");
    case CacheMaintenanceTarget::ImageLocation:
        return QCoreApplication::translate(
            "CacheMaintenanceDialog",
            "Saves place names found for photo locations so the app can reuse them without another online lookup.");
    case CacheMaintenanceTarget::Logs:
        return dialog_tr("Stores application log files used for troubleshooting and error reporting.");
    }
    return QString();
}
