#pragma once

#include <QString>
#include <QStringList>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <cstdint>
#include <ctime>

#include <spdlog/logger.h>

class UndoManager {
public:
    struct Entry {
        std::string source;
        std::string destination;
        std::uintmax_t size_bytes{0};
        std::time_t mtime{0};
    };

    explicit UndoManager(std::string undo_dir);

    bool save_plan(const std::string& run_base_dir,
                   const std::vector<Entry>& entries,
                   const std::shared_ptr<spdlog::logger>& logger) const;

    std::optional<QString> latest_plan_path() const;

    struct UndoResult {
        int restored{0};
        int skipped{0};
        QStringList details;
    };

    UndoResult undo_plan(const QString& plan_path) const;

private:
    std::string undo_dir_;
};
