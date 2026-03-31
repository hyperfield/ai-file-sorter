#include "OneDriveStorageProvider.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <iostream>
#include <string>

namespace {

constexpr auto kStoragePluginProtocol = "aifs-storage-plugin-v1";
constexpr auto kProviderId = "onedrive";

QJsonObject make_error_response(const QString& error_code,
                                const QString& message)
{
    QJsonObject response;
    response["protocol"] = QString::fromLatin1(kStoragePluginProtocol);
    response["success"] = false;
    response["error_code"] = error_code;
    response["error"] = message;
    return response;
}

QJsonObject detection_to_json(const StorageProviderDetection& detection)
{
    QJsonObject object;
    object["provider_id"] = QString::fromStdString(detection.provider_id);
    object["matched"] = detection.matched;
    object["needs_additional_support"] = detection.needs_additional_support;
    object["confidence"] = detection.confidence;
    object["message"] = QString::fromStdString(detection.message);
    return object;
}

QJsonObject capabilities_to_json(const StorageProviderCapabilities& capabilities)
{
    QJsonObject object;
    object["supports_online_only_files"] = capabilities.supports_online_only_files;
    object["supports_atomic_rename"] = capabilities.supports_atomic_rename;
    object["should_skip_reparse_points"] = capabilities.should_skip_reparse_points;
    object["should_relax_undo_mtime_validation"] =
        capabilities.should_relax_undo_mtime_validation;
    return object;
}

QJsonObject status_to_json(const StoragePathStatus& status)
{
    QJsonObject object;
    object["exists"] = status.exists;
    object["hydration_required"] = status.hydration_required;
    object["sync_locked"] = status.sync_locked;
    object["conflict_copy"] = status.conflict_copy;
    object["should_retry"] = status.should_retry;
    object["retry_after_ms"] = status.retry_after_ms;
    object["stable_identity"] = QString::fromStdString(status.stable_identity);
    object["revision_token"] = QString::fromStdString(status.revision_token);
    object["message"] = QString::fromStdString(status.message);
    return object;
}

QJsonObject preflight_to_json(const StorageMovePreflight& preflight)
{
    QJsonObject object;
    object["allowed"] = preflight.allowed;
    object["skipped"] = preflight.skipped;
    object["hydration_required"] = preflight.hydration_required;
    object["sync_locked"] = preflight.sync_locked;
    object["destination_conflict"] = preflight.destination_conflict;
    object["should_retry"] = preflight.should_retry;
    object["retry_after_ms"] = preflight.retry_after_ms;
    object["source_status"] = status_to_json(preflight.source_status);
    object["destination_status"] = status_to_json(preflight.destination_status);
    object["message"] = QString::fromStdString(preflight.message);
    return object;
}

QJsonObject metadata_to_json(const StorageEntryMetadata& metadata)
{
    QJsonObject object;
    object["size_bytes"] = static_cast<qint64>(metadata.size_bytes);
    object["mtime"] = static_cast<qint64>(metadata.mtime);
    object["stable_identity"] = QString::fromStdString(metadata.stable_identity);
    object["revision_token"] = QString::fromStdString(metadata.revision_token);
    return object;
}

QJsonObject mutation_to_json(const StorageMutationResult& mutation)
{
    QJsonObject object;
    object["mutation_success"] = mutation.success;
    object["skipped"] = mutation.skipped;
    object["message"] = QString::fromStdString(mutation.message);
    object["metadata"] = metadata_to_json(mutation.metadata);
    return object;
}

QJsonArray entries_to_json(const std::vector<FileEntry>& entries)
{
    QJsonArray array;
    for (const auto& entry : entries) {
        QJsonObject object;
        object["full_path"] = QString::fromStdString(entry.full_path);
        object["file_name"] = QString::fromStdString(entry.file_name);
        object["type"] = QString::fromStdString(to_string(entry.type));
        array.append(object);
    }
    return array;
}

void merge_json_object(QJsonObject& target, const QJsonObject& source)
{
    for (auto it = source.begin(); it != source.end(); ++it) {
        target.insert(it.key(), it.value());
    }
}

bool provider_id_is_valid(const QString& provider_id)
{
    return provider_id.isEmpty() || provider_id == QString::fromLatin1(kProviderId);
}

QJsonObject handle_request(const QJsonObject& request)
{
    const QString protocol = request.value("protocol").toString();
    if (!protocol.isEmpty() && protocol != QString::fromLatin1(kStoragePluginProtocol)) {
        return make_error_response(QStringLiteral("unsupported_protocol"),
                                   QStringLiteral("Unsupported storage plugin protocol."));
    }

    const QString provider_id = request.value("provider_id").toString();
    if (!provider_id_is_valid(provider_id)) {
        return make_error_response(QStringLiteral("unsupported_provider"),
                                   QStringLiteral("This connector only supports the OneDrive provider."));
    }

    const QString action = request.value("action").toString();
    if (action.isEmpty()) {
        return make_error_response(QStringLiteral("invalid_request"),
                                   QStringLiteral("Missing plugin action."));
    }

    OneDriveStorageProvider provider;
    QJsonObject response;
    response["protocol"] = QString::fromLatin1(kStoragePluginProtocol);
    response["success"] = true;

    if (action == QStringLiteral("probe")) {
        QJsonArray provider_ids;
        provider_ids.append(QString::fromLatin1(kProviderId));
        QJsonArray supported_protocols;
        supported_protocols.append(QString::fromLatin1(kStoragePluginProtocol));
        response["plugin_id"] = QStringLiteral("onedrive_storage_support");
        response["provider_ids"] = provider_ids;
        response["supported_protocols"] = supported_protocols;
        return response;
    }

    if (action == QStringLiteral("detect")) {
        response["detection"] =
            detection_to_json(provider.detect(request.value("root_path").toString().toStdString()));
        return response;
    }

    if (action == QStringLiteral("capabilities")) {
        response["capabilities"] = capabilities_to_json(provider.capabilities());
        return response;
    }

    if (action == QStringLiteral("list_directory")) {
        const auto options =
            static_cast<FileScanOptions>(request.value("options").toInt(0));
        response["entries"] = entries_to_json(
            provider.list_directory(request.value("directory").toString().toStdString(),
                                    options));
        return response;
    }

    if (action == QStringLiteral("inspect_path")) {
        response["status"] =
            status_to_json(provider.inspect_path(request.value("path").toString().toStdString()));
        return response;
    }

    if (action == QStringLiteral("preflight_move")) {
        response["preflight"] = preflight_to_json(
            provider.preflight_move(request.value("source").toString().toStdString(),
                                    request.value("destination").toString().toStdString()));
        return response;
    }

    if (action == QStringLiteral("path_exists")) {
        response["exists"] =
            provider.path_exists(request.value("path").toString().toStdString());
        return response;
    }

    if (action == QStringLiteral("ensure_directory")) {
        std::string error;
        if (!provider.ensure_directory(request.value("directory").toString().toStdString(), &error)) {
            return make_error_response(QStringLiteral("ensure_directory_failed"),
                                       QString::fromStdString(error.empty()
                                                                  ? "Failed to create directory."
                                                                  : error));
        }
        return response;
    }

    if (action == QStringLiteral("move_entry")) {
        const auto result =
            provider.move_entry(request.value("source").toString().toStdString(),
                                request.value("destination").toString().toStdString());
        merge_json_object(response, mutation_to_json(result));
        return response;
    }

    if (action == QStringLiteral("undo_move")) {
        const auto result =
            provider.undo_move(request.value("source").toString().toStdString(),
                               request.value("destination").toString().toStdString());
        merge_json_object(response, mutation_to_json(result));
        return response;
    }

    return make_error_response(QStringLiteral("unsupported_action"),
                               QStringLiteral("Unsupported OneDrive plugin action."));
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const std::string payload(std::istreambuf_iterator<char>(std::cin),
                              std::istreambuf_iterator<char>());
    const QJsonDocument request_document =
        QJsonDocument::fromJson(QByteArray::fromStdString(payload));
    if (!request_document.isObject()) {
        const auto response = make_error_response(QStringLiteral("invalid_request"),
                                                  QStringLiteral("Invalid JSON request."));
        std::cout << QJsonDocument(response).toJson(QJsonDocument::Compact).constData();
        return 1;
    }

    const QJsonObject response = handle_request(request_document.object());
    std::cout << QJsonDocument(response).toJson(QJsonDocument::Compact).constData();
    return response.value("success").toBool(false) ? 0 : 1;
}
