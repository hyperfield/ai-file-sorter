#include "SortingRulesDialog.hpp"

#include "Settings.hpp"

#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

namespace {
constexpr int kDialogWidth = 650;
constexpr int kDialogHeight = 640;
constexpr int kMinimumDialogWidth = 440;

QSlider* make_word_slider(QWidget* parent, const QString& name, int value)
{
    auto* slider = new QSlider(Qt::Horizontal, parent);
    slider->setObjectName(name);
    slider->setRange(kMinimumFilenameWords, kMaximumFilenameWords);
    slider->setSingleStep(1);
    slider->setPageStep(1);
    slider->setTickInterval(1);
    slider->setTickPosition(QSlider::TicksBelow);
    slider->setValue(value);
    return slider;
}

QWidget* make_slider_row(QSlider* slider, QLabel* value, QWidget* parent)
{
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    value->setMinimumWidth(value->fontMetrics().horizontalAdvance(QStringLiteral("6 words")));
    layout->addWidget(slider, 1);
    layout->addWidget(value);
    return row;
}
}

SortingRulesDialog::SortingRulesDialog(Settings& settings, QWidget* parent)
    : QDialog(parent), settings_(settings)
{
    setObjectName(QStringLiteral("sortingRulesDialog"));
    resize(kDialogWidth, kDialogHeight);
    setMinimumWidth(kMinimumDialogWidth);

    auto* layout = new QVBoxLayout(this);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget(scroll);
    auto* content_layout = new QVBoxLayout(content);
    rules_widget_ = new QWidget(content);
    content_layout->addWidget(rules_widget_);

    filename_group_ = new QGroupBox(content);
    auto* form = new QFormLayout(filename_group_);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    image_label_ = new QLabel(filename_group_);
    document_label_ = new QLabel(filename_group_);
    image_value_ = new QLabel(filename_group_);
    document_value_ = new QLabel(filename_group_);
    image_slider_ = make_word_slider(filename_group_, QStringLiteral("imageFilenameWordsSlider"),
                                     settings.get_image_filename_words());
    document_slider_ = make_word_slider(filename_group_, QStringLiteral("documentFilenameWordsSlider"),
                                        settings.get_document_filename_words());
    image_label_->setBuddy(image_slider_);
    document_label_->setBuddy(document_slider_);
    form->addRow(image_label_, make_slider_row(image_slider_, image_value_, filename_group_));
    form->addRow(document_label_, make_slider_row(document_slider_, document_value_, filename_group_));
    content_layout->addWidget(filename_group_);
    content_layout->addStretch();
    scroll->setWidget(content);
    layout->addWidget(scroll);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(image_slider_, &QSlider::valueChanged, this, [this, &settings](int words) {
        settings.set_image_filename_words(words);
        retranslate();
    });
    connect(document_slider_, &QSlider::valueChanged, this, [this, &settings](int words) {
        settings.set_document_filename_words(words);
        retranslate();
    });
    retranslate();
}

QWidget* SortingRulesDialog::rules_widget() const
{
    return rules_widget_;
}

void SortingRulesDialog::changeEvent(QEvent* event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
    }
}

void SortingRulesDialog::showEvent(QShowEvent* event)
{
    const QSignalBlocker image_blocker(image_slider_);
    const QSignalBlocker document_blocker(document_slider_);
    image_slider_->setValue(settings_.get_image_filename_words());
    document_slider_->setValue(settings_.get_document_filename_words());
    retranslate();
    QDialog::showEvent(event);
}

void SortingRulesDialog::retranslate()
{
    setWindowTitle(tr("Sorting Rules"));
    filename_group_->setTitle(tr("Maximum filename words"));
    image_label_->setText(tr("Images"));
    document_label_->setText(tr("Documents"));
    image_value_->setText(tr("%1 words").arg(image_slider_->value()));
    document_value_->setText(tr("%1 words").arg(document_slider_->value()));
    image_slider_->setAccessibleName(tr("Image filename word limit"));
    document_slider_->setAccessibleName(tr("Document filename word limit"));
}
