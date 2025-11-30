#pragma once

#include <QDialog>
#include <QTableWidget>

#include <string>
#include <vector>

class DryRunPreviewDialog : public QDialog {
public:
    struct Entry {
        std::string from_label;
        std::string to_label;
        std::string source_tooltip;
        std::string destination_tooltip;
    };

    explicit DryRunPreviewDialog(const std::vector<Entry>& entries, QWidget* parent = nullptr);

private:
    void setup_ui(const std::vector<Entry>& entries);
    QTableWidget* table_{nullptr};
};
