/**
 * @file CacheMaintenanceDialog.hpp
 * @brief Settings dialog for clearing cache and log data.
 */
#pragma once

#include <QCoreApplication>
#include <QDialog>

#include <map>

class QLabel;
class QPushButton;
class QScrollArea;
class QWidget;

class CacheMaintenanceService;
enum class CacheMaintenanceTarget;

/**
 * @brief Modal dialog that lets users inspect and clear individual cache targets.
 */
class CacheMaintenanceDialog : public QDialog {
    Q_DECLARE_TR_FUNCTIONS(CacheMaintenanceDialog)

public:
    /**
     * @brief Constructs the cache maintenance dialog.
     * @param service Cache maintenance service used for inspection and clearing.
     * @param busy True when cleanup actions should be disabled.
     * @param parent Optional parent widget.
     */
    explicit CacheMaintenanceDialog(CacheMaintenanceService& service,
                                    bool busy,
                                    QWidget* parent = nullptr);

private:
    struct TargetWidgets {
        QLabel* title_label{nullptr};
        QLabel* description_label{nullptr};
        QLabel* path_value{nullptr};
        QLabel* size_value{nullptr};
        QPushButton* clear_button{nullptr};
    };

    /**
     * @brief Adds the UI block for a single cache target.
     * @param target Cache target represented by the block.
     */
    void add_target_section(CacheMaintenanceTarget target);
    /**
     * @brief Refreshes the displayed path/size metadata for a cache target.
     * @param target Cache target to refresh.
     */
    void refresh_target(CacheMaintenanceTarget target);
    /**
     * @brief Handles the clear action for a cache target.
     * @param target Cache target to clear.
     */
    void clear_target(CacheMaintenanceTarget target);
    /**
     * @brief Returns the localized title for a cache target.
     * @param target Cache target to label.
     * @return Localized display title.
     */
    QString target_title(CacheMaintenanceTarget target) const;
    /**
     * @brief Returns the localized tooltip/description for a cache target.
     * @param target Cache target to describe.
     * @return Localized explanatory text.
     */
    QString target_description(CacheMaintenanceTarget target) const;

    CacheMaintenanceService& service_;
    bool busy_{false};
    QLabel* busy_label_{nullptr};
    QWidget* content_widget_{nullptr};
    QScrollArea* content_scroll_area_{nullptr};
    class QVBoxLayout* content_layout_{nullptr};
    std::map<CacheMaintenanceTarget, TargetWidgets> target_widgets_;
};
