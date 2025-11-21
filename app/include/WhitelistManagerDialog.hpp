#ifndef WHITELIST_MANAGER_DIALOG_HPP
#define WHITELIST_MANAGER_DIALOG_HPP

#include <QDialog>
#include <QPointer>
#include <QString>
#include <functional>
#include "WhitelistStore.hpp"

class QListWidget;
class QTextEdit;
class QLineEdit;
class QPushButton;
class QLabel;

class WhitelistManagerDialog : public QDialog {
public:
    explicit WhitelistManagerDialog(WhitelistStore& store, QWidget* parent = nullptr);

    void set_on_lists_changed(std::function<void()> cb) { on_lists_changed_ = std::move(cb); }

private:
    void on_add_clicked();
    void on_edit_clicked();
    void on_remove_clicked();
    void on_selection_changed(int row);

    void refresh_list();
    bool edit_entry(const QString& name, WhitelistEntry& entry);
    void notify_changed();

    WhitelistStore& store_;
    QPointer<QListWidget> list_widget_;
    QPointer<QPushButton> add_button_;
    QPointer<QPushButton> edit_button_;
    QPointer<QPushButton> remove_button_;
    std::function<void()> on_lists_changed_;
};

#endif
