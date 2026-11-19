#include "JohnnyDecimalValidationHighlighter.hpp"

#include <QColor>
#include <QCoreApplication>
#include <QEvent>
#include <QFont>
#include <QPalette>
#include <QPlainTextEdit>
#include <QTextCharFormat>

#include "JohnnyDecimalValidator.hpp"

namespace {

constexpr int kDarkPaletteThreshold = 128;
constexpr auto kErrorLight = "#c62828";
constexpr auto kErrorDark = "#ff8a80";
constexpr auto kWarningText = "#ffdf66";
constexpr auto kWarningBackground = "#493900";
constexpr auto kInfoLight = "#1565c0";
constexpr auto kInfoDark = "#90caf9";
constexpr auto kCodeLight = "#616161";
constexpr auto kCodeDark = "#bdbdbd";

QTextCharFormat severity_format(JohnnyDecimalValidator::Severity severity, bool dark) {
    QTextCharFormat format;
    format.setFontWeight(QFont::Bold);
    switch (severity) {
        case JohnnyDecimalValidator::Severity::Error:
            format.setForeground(QColor(dark ? kErrorDark : kErrorLight));
            break;
        case JohnnyDecimalValidator::Severity::Warning:
            format.setForeground(QColor(kWarningText));
            format.setBackground(QColor(kWarningBackground));
            break;
        case JohnnyDecimalValidator::Severity::Info:
            format.setForeground(QColor(dark ? kInfoDark : kInfoLight));
            break;
    }
    return format;
}

}  // namespace

JohnnyDecimalValidationHighlighter::JohnnyDecimalValidationHighlighter(QPlainTextEdit* editor)
    : QSyntaxHighlighter(editor->document()), editor_(editor) {
    setObjectName(QStringLiteral("johnnyDecimalValidationHighlighter"));
    editor->installEventFilter(this);
}

void JohnnyDecimalValidationHighlighter::highlightBlock(const QString& text) {
    const bool dark = editor_ && editor_->palette().color(QPalette::Base).lightness() < kDarkPaletteThreshold;
    QTextCharFormat bold;
    bold.setFontWeight(QFont::Bold);
    for (const auto severity : {JohnnyDecimalValidator::Severity::Error,
                               JohnnyDecimalValidator::Severity::Warning,
                               JohnnyDecimalValidator::Severity::Info}) {
        const auto label = QString::fromStdString(JohnnyDecimalValidator::severity_label(severity));
        if (!text.startsWith(label + QStringLiteral(" - "))) {
            continue;
        }
        setFormat(0, text.size(), bold);
        setFormat(0, label.size(), severity_format(severity, dark));
        const auto code_start = text.lastIndexOf(QStringLiteral(" ["));
        if (code_start >= 0 && text.endsWith(QLatin1Char(']'))) {
            QTextCharFormat code;
            code.setForeground(QColor(dark ? kCodeDark : kCodeLight));
            setFormat(code_start, text.size() - code_start, code);
        }
        return;
    }
    for (const auto& label : {QStringLiteral("Root:"), QStringLiteral("Path:"),
                              QStringLiteral("Details:"), QStringLiteral("Suggested fix:")}) {
        if (text.startsWith(label)) {
            setFormat(0, label.size(), bold);
            return;
        }
    }
    const auto items_label = QCoreApplication::translate("JohnnyDecimalValidator", "Item folders checked: %1")
                                 .arg(QString());
    if (text == QStringLiteral("Johnny.Decimal validation report") ||
        text.startsWith(QStringLiteral("Areas:")) || text.startsWith(QStringLiteral("Errors:")) ||
        text.startsWith(items_label)) {
        setFormat(0, text.size(), bold);
    }
}

bool JohnnyDecimalValidationHighlighter::eventFilter(QObject* watched, QEvent* event) {
    if (watched == editor_ && (event->type() == QEvent::PaletteChange ||
                               event->type() == QEvent::ApplicationPaletteChange)) {
        rehighlight();
    }
    return QSyntaxHighlighter::eventFilter(watched, event);
}
