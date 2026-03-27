#include "UndoManager.hpp"

#include "LocalFsProvider.hpp"
#include "StorageProviderRegistry.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QMessageBox>
#include <spdlog/logger.h>

#include <fmt/format.h>

UndoManager::UndoManager(std::string undo_dir,
                         const StorageProviderRegistry* storage_provider_registry)
    : undo_dir_(std::move(undo_dir)),
      storage_provider_registry_(storage_provider_registry)
{}

bool UndoManager::save_plan(const std::string& run_base_dir,
                            const std::string& provider_id,
                            const std::vector<Entry>& entries,
                            const std::shared_ptr<spdlog::logger>& logger) const
{
    if (undo_dir_.empty() || entries.empty()) {
        return false;
    }

    QDir dir(QString::fromStdString(undo_dir_));
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    QJsonArray arr;
    for (const auto& entry : entries) {
        QJsonObject obj;
        obj["source"] = QString::fromStdString(entry.source);
        obj["destination"] = QString::fromStdString(entry.destination);
        obj["size"] = static_cast<qint64>(entry.size_bytes);
        obj["mtime"] = static_cast<qint64>(entry.mtime);
        arr.push_back(obj);
    }

    QJsonObject root;
    root["version"] = 1;
    root["base_dir"] = QString::fromStdString(run_base_dir);
    root["provider_id"] = QString::fromStdString(provider_id);
    root["created_at_utc"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    root["entries"] = arr;

    const QString filename = QStringLiteral("undo_plan_%1.json")
        .arg(QDateTime::currentDateTimeUtc().toString("yyyyMMdd_hhmmsszzz"));
    QFile file(dir.filePath(filename));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (logger) {
            logger->error("Failed to write undo plan '{}': {}", filename.toStdString(), file.errorString().toStdString());
        }
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    if (logger) {
        logger->info("Saved undo plan to '{}'", file.fileName().toStdString());
    }
    return true;
}

std::optional<QString> UndoManager::latest_plan_path() const
{
    if (undo_dir_.empty()) {
        return std::nullopt;
    }
    QDir dir(QString::fromStdString(undo_dir_));
    if (!dir.exists()) {
        return std::nullopt;
    }
    const auto files = dir.entryInfoList(QStringList() << "undo_plan_*.json",
                                         QDir::Files,
                                         QDir::Time | QDir::Reversed);
    if (files.isEmpty()) {
        return std::nullopt;
    }
    return files.back().filePath();
}

UndoManager::UndoResult UndoManager::undo_plan(const QString& plan_path) const
{
    UndoResult result;

    QFile file(plan_path);
    if (!file.open(QIODevice::ReadOnly)) {
        result.details << QString("Failed to open plan: %1").arg(plan_path);
        result.skipped++;
        return result;
    }

    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        result.details << QString("Invalid plan: %1").arg(plan_path);
        result.skipped++;
        return result;
    }

    const QJsonObject root = doc.object();
    const QJsonArray entries = root.value("entries").toArray();
    const std::string provider_id =
        root.value("provider_id").toString(QStringLiteral("local_fs")).toStdString();
    LocalFsProvider fallback_provider;
    std::shared_ptr<IStorageProvider> resolved_provider =
        storage_provider_registry_ ? storage_provider_registry_->find_by_id(provider_id) : nullptr;
    IStorageProvider* provider = nullptr;
    if (resolved_provider) {
        provider = resolved_provider.get();
    } else if (provider_id.empty() || provider_id == "local_fs") {
        provider = &fallback_provider;
    } else {
        result.details << QString("Missing storage provider: %1")
                              .arg(QString::fromStdString(provider_id));
        result.skipped += entries.size();
        return result;
    }
    for (const auto& val : entries) {
        if (!val.isObject()) {
            result.skipped++;
            continue;
        }
        const QJsonObject obj = val.toObject();
        const QString source = obj.value("source").toString();
        const QString destination = obj.value("destination").toString();
        const qint64 expected_size = obj.value("size").toInteger(0);
        const qint64 expected_mtime = obj.value("mtime").toInteger(0);

        QFileInfo dest_info(destination);
        if (!dest_info.exists()) {
            result.details << QString("Missing destination: %1").arg(destination);
            result.skipped++;
            continue;
        }

        QFileInfo src_info(source);
        if (src_info.exists()) {
            result.details << QString("Source already exists, skipping: %1").arg(source);
            result.skipped++;
            continue;
        }

        if (expected_size > 0 && dest_info.size() != expected_size) {
            result.details << QString("Size mismatch for %1").arg(destination);
            result.skipped++;
            continue;
        }

        if (expected_mtime > 0) {
            const auto mtime = dest_info.lastModified().toSecsSinceEpoch();
            if (mtime != expected_mtime) {
                result.details << QString("Timestamp mismatch for %1").arg(destination);
                result.skipped++;
                continue;
            }
        }

        const auto undo_result = provider->undo_move(source.toStdString(), destination.toStdString());
        if (undo_result.success) {
            result.restored++;
        } else {
            const QString detail = undo_result.message.empty()
                ? QString("Failed to move %1 back to %2").arg(destination, source)
                : QString::fromStdString(undo_result.message);
            result.details << detail;
            result.skipped++;
        }
    }

    return result;
}
