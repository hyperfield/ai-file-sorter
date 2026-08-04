#pragma once

#include "ReviewHistoryStore.hpp"

#include <QCoreApplication>
#include <QDialog>
#include <QString>
#include <QStringList>

#include <vector>

class IStorageProvider;
class QEvent;
class QLineEdit;
class QPushButton;
class QStandardItemModel;
class QTableView;

/**
 * @brief Shows searchable rename/categorization history and supports undoing selected rows.
 */
class ReviewHistoryDialog : public QDialog {
    Q_DECLARE_TR_FUNCTIONS(ReviewHistoryDialog)

public:
    /**
     * @brief Creates a review history dialog.
     * @param store Persistent history store to read and update.
     * @param storage_provider Storage provider used for undo operations.
     * @param parent Parent widget.
     */
    ReviewHistoryDialog(ReviewHistoryStore& store,
                        IStorageProvider& storage_provider,
                        QWidget* parent = nullptr);

protected:
    /**
     * @brief Retranslates visible text after language changes.
     * @param event Qt change event.
     */
    void changeEvent(QEvent* event) override;

private:
    enum Column {
        ColumnDate = 0,
        ColumnOperation,
        ColumnOriginalName,
        ColumnFinalName,
        ColumnCategory,
        ColumnDescription,
        ColumnStatus,
        ColumnSource,
        ColumnDestination,
        ColumnCount
    };

    static constexpr int kEntryIdRole = Qt::UserRole + 1;
    static constexpr int kUndoneRole = Qt::UserRole + 2;

    /**
     * @brief Builds widgets, layout, and signal connections.
     */
    void setup_ui();
    /**
     * @brief Updates translatable labels and table headings.
     */
    void retranslate_ui();
    /**
     * @brief Reloads rows from the history store using the current search text.
     */
    void load_entries();
    /**
     * @brief Adds one persisted history entry to the table model.
     * @param entry History row to display.
     */
    void append_entry_row(const ReviewHistoryStore::Entry& entry);
    /**
     * @brief Attempts to undo the currently selected history rows.
     */
    void undo_selected_entries();
    /**
     * @brief Enables the undo button only when undoable rows are selected.
     */
    void update_undo_button_state();
    /**
     * @brief Updates one visible row after a successful undo.
     * @param id History row id.
     */
    void mark_row_undone(long long id);
    /**
     * @brief Returns unique selected history row ids.
     * @return Selected ids.
     */
    std::vector<long long> selected_entry_ids() const;
    /**
     * @brief Formats an operation for display.
     * @param operation Stored operation value.
     * @return Localized display text.
     */
    QString display_operation(ReviewHistoryStore::Operation operation) const;
    /**
     * @brief Formats the category/subcategory pair for display.
     * @param entry History row.
     * @return Category label.
     */
    QString display_category(const ReviewHistoryStore::Entry& entry) const;
    /**
     * @brief Formats a UTC timestamp for the user's locale.
     * @param created_at_utc Stored UTC timestamp.
     * @return Local display text.
     */
    QString display_date(const std::string& created_at_utc) const;
    /**
     * @brief Validates whether one history entry can currently be undone.
     * @param entry History row to validate.
     * @param details Receives human-readable blocking reasons.
     * @return True when undo can proceed.
     */
    bool can_undo_entry(const ReviewHistoryStore::Entry& entry, QStringList& details) const;

    ReviewHistoryStore& store_;
    IStorageProvider& storage_provider_;
    QLineEdit* search_edit_{nullptr};
    QTableView* table_view_{nullptr};
    QStandardItemModel* model_{nullptr};
    QPushButton* undo_button_{nullptr};
    QPushButton* close_button_{nullptr};
};
