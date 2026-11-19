#pragma once

#include <QPointer>
#include <QSyntaxHighlighter>

class QEvent;
class QPlainTextEdit;

/** @brief Adds accessible severity colors and headings to plain-text validation reports. */
class JohnnyDecimalValidationHighlighter : public QSyntaxHighlighter {
public:
    /**
     * @brief Attaches report formatting without changing selectable or copied text.
     * @param editor Non-null report editor whose document owns this highlighter.
     */
    explicit JohnnyDecimalValidationHighlighter(QPlainTextEdit* editor);

protected:
    /**
     * @brief Formats report headings, severity labels, field labels, and diagnostic codes.
     * @param text Current document block.
     */
    void highlightBlock(const QString& text) override;

    /**
     * @brief Refreshes severity colors when the editor's palette changes.
     * @param watched Object receiving the event.
     * @param event Palette or other editor event.
     * @return Whether the event was consumed.
     */
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QPointer<QPlainTextEdit> editor_;
};
