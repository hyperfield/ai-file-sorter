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

/**
 * @brief Dialog for creating, editing, and deleting whitelist entries.
 */
class WhitelistManagerDialog : public QDialog {
public:
    /**
     * @brief Constructs the whitelist manager dialog.
     * @param store Backing store for whitelist entries.
     * @param parent Optional parent widget.
     */
    explicit WhitelistManagerDialog(WhitelistStore& store, QWidget* parent = nullptr);

    /**
     * @brief Registers a callback invoked when the whitelist collection changes.
     * @param cb Callback to invoke after changes.
     */
    void set_on_lists_changed(std::function<void()> cb) { on_lists_changed_ = std::move(cb); }

private:
    /**
     * @brief Handles the Add button action.
     */
    void on_add_clicked();
    /**
     * @brief Handles the Edit button action.
     */
    void on_edit_clicked();
    /**
     * @brief Handles the Remove button action.
     */
    void on_remove_clicked();
    /**
     * @brief Handles selection change events.
     * @param row Selected row index.
     */
    void on_selection_changed(int row);

    /**
     * @brief Refreshes the list widget from the store.
     */
    void refresh_list();
    /**
     * @brief Opens the editor UI for a whitelist entry.
     * @param name Current whitelist name.
     * @param entry Entry to edit (modified on success).
     * @return True when the entry was saved.
     */
    bool edit_entry(const QString& name, WhitelistEntry& entry);
    /**
     * @brief Notifies observers that the whitelist collection changed.
     */
    void notify_changed();

    /**
     * @brief Backing store for whitelist data.
     */
    WhitelistStore& store_;
    /**
     * @brief List widget showing available whitelists.
     */
    QPointer<QListWidget> list_widget_;
    /**
     * @brief Add button pointer.
     */
    QPointer<QPushButton> add_button_;
    /**
     * @brief Edit button pointer.
     */
    QPointer<QPushButton> edit_button_;
    /**
     * @brief Remove button pointer.
     */
    QPointer<QPushButton> remove_button_;
    /**
     * @brief Callback invoked after list changes.
     */
    std::function<void()> on_lists_changed_;
};

#endif
