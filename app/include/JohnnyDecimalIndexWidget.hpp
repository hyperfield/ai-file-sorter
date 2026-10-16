#pragma once

#include <QCoreApplication>
#include <QWidget>
#include <functional>
#include <map>

#include "JohnnyDecimalArchiveIndex.hpp"
#include "JohnnyDecimalItemAnalysis.hpp"

class QLabel;
class QLineEdit;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
class QUrl;

/** @brief Searchable, read-only index for enabled Johnny.Decimal plugin profiles. */
class JohnnyDecimalIndexWidget : public QWidget {
    Q_DECLARE_TR_FUNCTIONS(JohnnyDecimalIndexWidget)

public:
    /** @brief Folder-opening service, injectable without starting a desktop application in tests. */
    using OpenFolder = std::function<bool(const QUrl&)>;

    /**
     * @brief Creates the index controls.
     * @param parent Owning widget.
     * @param open_folder Optional service for opening a local folder URL.
     */
    explicit JohnnyDecimalIndexWidget(QWidget* parent = nullptr, OpenFolder open_folder = {});

    /**
     * @brief Replaces the displayed archive snapshot and recomputes conflict markers.
     * @param index Complete archive snapshot; filters affect only its display.
     */
    void set_index(JohnnyDecimalArchiveIndex::Index index);

private:
    /** @brief Rebuilds the tree from the current snapshot. */
    void populate();
    /** @brief Applies the search filter while keeping matching ancestors visible. */
    void filter();
    /** @brief Updates open/export action availability for the current selection. */
    void update_actions();
    /** @brief Opens the selected folder after confirming it still belongs to the archive. */
    void open_selected();
    /** @brief Prompts for and atomically writes a complete Markdown index. */
    void export_index();
    /**
     * @brief Adds a folder row, retaining its relative path independently of displayed labels.
     * @param parent Parent row, or null for a top-level row.
     * @param id Displayed number or ID.
     * @param label Human-readable name.
     * @param relative_path Archive-relative path.
     * @param status Optional findings displayed on this row.
     * @return Added row.
     */
    QTreeWidgetItem* add_folder(QTreeWidgetItem* parent, const std::string& id, const std::string& label,
                               const std::string& relative_path, const QString& status = {});
    /**
     * @brief Adds an item and its ordinary descendant folders.
     * @param parent Category or item row.
     * @param item Indexed item subtree.
     */
    void add_item(QTreeWidgetItem* parent, const JohnnyDecimalArchiveIndex::Item& item);
    /**
     * @brief Formats item findings for the tree status column.
     * @param item Analysis findings for this direct item folder.
     * @return Localized list of findings.
     */
    QString item_status(const JohnnyDecimalItemAnalysis::Item& item) const;

    JohnnyDecimalArchiveIndex::Index index_;
    std::map<std::string, JohnnyDecimalItemAnalysis::Item> item_findings_;
    OpenFolder open_folder_;
    QLineEdit* search_{nullptr};
    QTreeWidget* tree_{nullptr};
    QLabel* summary_{nullptr};
    QLabel* message_{nullptr};
    QToolButton* open_{nullptr};
    QToolButton* export_{nullptr};
};
