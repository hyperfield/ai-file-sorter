#include "MainAppEditActions.hpp"

#include <QClipboard>
#include <QGuiApplication>
#include <QLineEdit>

void MainAppEditActions::on_paste(QLineEdit* line_edit)
{
    if (!line_edit) {
        return;
    }
    const QString clipboard_text = QGuiApplication::clipboard()->text(QClipboard::Clipboard);
    if (!clipboard_text.isEmpty()) {
        line_edit->insert(clipboard_text);
    }
}


void MainAppEditActions::on_copy(QLineEdit* line_edit)
{
    if (!line_edit) {
        return;
    }
    const QString selected_text = get_selection(line_edit, false);
    if (!selected_text.isEmpty()) {
        copy_to_clipboard(selected_text);
    }
}


void MainAppEditActions::on_cut(QLineEdit* line_edit)
{
    if (!line_edit) {
        return;
    }
    const QString selected_text = get_selection(line_edit, true);
    if (!selected_text.isEmpty()) {
        copy_to_clipboard(selected_text);
    }
}


void MainAppEditActions::on_delete(QLineEdit* line_edit)
{
    if (!line_edit) {
        return;
    }
    get_selection(line_edit, true);
}


void MainAppEditActions::copy_to_clipboard(const QString& text)
{
    QGuiApplication::clipboard()->setText(text, QClipboard::Clipboard);
}


QString MainAppEditActions::get_selection(QLineEdit* line_edit, bool delete_selection)
{
    if (!line_edit) {
        return {};
    }

    const QString selected_text = line_edit->selectedText();
    if (selected_text.isEmpty()) {
        return {};
    }

    if (delete_selection) {
        line_edit->insert(QString());
    }

    return selected_text;
}
