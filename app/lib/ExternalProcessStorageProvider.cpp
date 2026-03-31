#include "ExternalProcessStorageProvider.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>

#include <algorithm>
#include <filesystem>

namespace {

constexpr auto kStoragePluginProtocol = "aifs-storage-plugin-v1";

QString path_to_program(const StoragePluginManifest& manifest)
{
    const std::filesystem::path entry_path(manifest.entry_point);
    if (entry_path.is_absolute()) {
        return QString::fromStdString(entry_path.string());
    }

    if (!manifest.source_path.empty()) {
        const auto relative_candidate = manifest.source_path.parent_path() / entry_path;
        std::error_code ec;
        if (std::filesystem::exists(relative_candidate, ec) && !ec) {
            return QString::fromStdString(relative_candidate.string());
        }
    }

    const QString app_dir = QCoreApplication::applicationDirPath();
    if (!app_dir.isEmpty()) {
        const auto app_candidate =
            std::filesystem::path(app_dir.toStdString()) / entry_path;
        std::error_code ec;
        if (std::filesystem::exists(app_candidate, ec) && !ec) {
            return QString::fromStdString(app_candidate.string());
        }
    }

    const QString discovered = QStandardPaths::findExecutable(QString::fromStdString(manifest.entry_point));
    if (!discovered.isEmpty()) {
        return discovered;
    }

    return QString::fromStdString(manifest.entry_point);
}

bool invoke_process(const StoragePluginManifest& manifest,
                    const QJsonObject& request,
                    QJsonObject* response,
                    std::string* error)
{
    const QString program = path_to_program(manifest);
    if (program.isEmpty()) {
        if (error) {
            *error = "Plugin entry point is empty.";
        }
        return false;
    }

    QProcess process;
    if (!manifest.source_path.empty()) {
        process.setWorkingDirectory(QString::fromStdString(manifest.source_path.parent_path().string()));
    }

    process.start(program, {});
    if (!process.waitForStarted(3000)) {
        if (error) {
            *error = process.errorString().toStdString();
        }
        return false;
    }

    const QByteArray payload = QJsonDocument(request).toJson(QJsonDocument::Compact);
    if (process.write(payload) != payload.size()) {
        if (error) {
            *error = "Failed to send request to storage plugin process.";
        }
        process.kill();
        process.waitForFinished(1000);
        return false;
    }
    process.closeWriteChannel();

    if (!process.waitForFinished(5000)) {
        if (error) {
            *error = "Storage plugin process timed out.";
        }
        process.kill();
        process.waitForFinished(1000);
        return false;
    }

    const QByteArray stderr_data = process.readAllStandardError();
    const QByteArray stdout_data = process.readAllStandardOutput();
    const QJsonDocument doc = QJsonDocument::fromJson(stdout_data);
    if (!doc.isObject()) {
        if (error) {
            const QString stderr_text = QString::fromUtf8(stderr_data).trimmed();
            *error = stderr_text.isEmpty()
                ? "Storage plugin process returned invalid JSON."
                : stderr_text.toStdString();
        }
        return false;
    }

    *response = doc.object();
    const QString response_protocol = response->value("protocol").toString();
    if (!response_protocol.isEmpty() &&
        response_protocol != QString::fromLatin1(kStoragePluginProtocol)) {
        if (error) {
            *error = "Storage plugin process reported an unsupported protocol version.";
        }
        return false;
    }
    if (!response->value("success").toBool(true)) {
        if (error) {
            const QString response_error = response->value("error").toString();
            const QString response_code = response->value("error_code").toString();
            const QString detailed_error = response_code.isEmpty() || response_error.isEmpty()
                ? response_error
                : response_code + QStringLiteral(": ") + response_error;
            *error = detailed_error.isEmpty()
                ? "Storage plugin process returned an error."
                : detailed_error.toStdString();
        }
        return false;
    }

    return true;
}

QJsonObject make_request(const StoragePluginManifest& manifest,
                         const std::string& provider_id,
                         const char* action)
{
    QJsonObject request;
    request["protocol"] = QString::fromLatin1(kStoragePluginProtocol);
    request["plugin_id"] = QString::fromStdString(manifest.id);
    request["provider_id"] = QString::fromStdString(provider_id);
    request["action"] = QString::fromLatin1(action);
    return request;
}

StorageProviderCapabilities capabilities_from_json(const QJsonObject& object)
{
    return StorageProviderCapabilities{
        .supports_online_only_files = object.value("supports_online_only_files").toBool(false),
        .supports_atomic_rename = object.value("supports_atomic_rename").toBool(true),
        .should_skip_reparse_points = object.value("should_skip_reparse_points").toBool(false),
        .should_relax_undo_mtime_validation = object.value("should_relax_undo_mtime_validation").toBool(false)
    };
}

StoragePathStatus status_from_json(const QJsonObject& object)
{
    return StoragePathStatus{
        .exists = object.value("exists").toBool(false),
        .hydration_required = object.value("hydration_required").toBool(false),
        .sync_locked = object.value("sync_locked").toBool(false),
        .conflict_copy = object.value("conflict_copy").toBool(false),
        .should_retry = object.value("should_retry").toBool(false),
        .retry_after_ms = object.value("retry_after_ms").toInt(0),
        .stable_identity = object.value("stable_identity").toString().toStdString(),
        .revision_token = object.value("revision_token").toString().toStdString(),
        .message = object.value("message").toString().toStdString()
    };
}

StorageMovePreflight preflight_from_json(const QJsonObject& object)
{
    return StorageMovePreflight{
        .allowed = object.value("allowed").toBool(true),
        .skipped = object.value("skipped").toBool(false),
        .hydration_required = object.value("hydration_required").toBool(false),
        .sync_locked = object.value("sync_locked").toBool(false),
        .destination_conflict = object.value("destination_conflict").toBool(false),
        .should_retry = object.value("should_retry").toBool(false),
        .retry_after_ms = object.value("retry_after_ms").toInt(0),
        .source_status = status_from_json(object.value("source_status").toObject()),
        .destination_status = status_from_json(object.value("destination_status").toObject()),
        .message = object.value("message").toString().toStdString()
    };
}

FileType file_type_from_string(const QString& value)
{
    if (value.compare(QStringLiteral("Directory"), Qt::CaseInsensitive) == 0) {
        return FileType::Directory;
    }
    return FileType::File;
}

std::vector<FileEntry> entries_from_json(const QJsonArray& entries)
{
    std::vector<FileEntry> output;
    output.reserve(static_cast<std::size_t>(entries.size()));
    for (const auto& value : entries) {
        const QJsonObject object = value.toObject();
        output.push_back(FileEntry{
            .full_path = object.value("full_path").toString().toStdString(),
            .file_name = object.value("file_name").toString().toStdString(),
            .type = file_type_from_string(object.value("type").toString())
        });
    }
    return output;
}

StorageMutationResult mutation_from_json(const QJsonObject& response)
{
    StorageMutationResult result;
    result.success = response.value("mutation_success").toBool(false);
    result.skipped = response.value("skipped").toBool(false);
    result.message = response.value("message").toString().toStdString();

    const QJsonObject metadata = response.value("metadata").toObject();
    result.metadata.size_bytes = static_cast<std::uintmax_t>(
        metadata.value("size_bytes").toVariant().toULongLong());
    result.metadata.mtime = static_cast<std::time_t>(
        metadata.value("mtime").toVariant().toLongLong());
    result.metadata.stable_identity = metadata.value("stable_identity").toString().toStdString();
    result.metadata.revision_token = metadata.value("revision_token").toString().toStdString();
    return result;
}

} // namespace

ExternalProcessStorageProvider::ExternalProcessStorageProvider(StoragePluginManifest manifest,
                                                               std::string provider_id,
                                                               std::string instance_id,
                                                               bool requires_installation)
    : manifest_(std::move(manifest)),
      provider_id_(std::move(provider_id)),
      instance_id_(std::move(instance_id)),
      requires_installation_(requires_installation)
{
}

bool ExternalProcessStorageProvider::validate_plugin_manifest(const StoragePluginManifest& manifest,
                                                              std::string* error)
{
    QJsonObject request = make_request(manifest, std::string(), "probe");
    QJsonObject response;
    if (!invoke_process(manifest, request, &response, error)) {
        return false;
    }

    const QJsonArray supported_provider_ids = response.value("provider_ids").toArray();
    if (!supported_provider_ids.isEmpty()) {
        std::vector<std::string> supported;
        supported.reserve(static_cast<std::size_t>(supported_provider_ids.size()));
        for (const auto& value : supported_provider_ids) {
            supported.push_back(value.toString().toStdString());
        }
        for (const auto& provider_id : manifest.provider_ids) {
            if (std::find(supported.begin(), supported.end(), provider_id) == supported.end()) {
                if (error) {
                    *error = "Storage plugin process does not expose provider '" + provider_id + "'.";
                }
                return false;
            }
        }
    }

    return true;
}

std::string ExternalProcessStorageProvider::id() const
{
    return instance_id_;
}

StorageProviderDetection ExternalProcessStorageProvider::detect(const std::string& root_path) const
{
    QJsonObject request = make_request(manifest_, provider_id_, "detect");
    request["root_path"] = QString::fromStdString(root_path);

    QJsonObject response;
    std::string error;
    if (!invoke_process(manifest_, request, &response, &error)) {
        return {};
    }

    const QJsonObject detection = response.value("detection").toObject();
    if (!detection.value("matched").toBool(false)) {
        return {};
    }

    return StorageProviderDetection{
        .provider_id = provider_id_,
        .matched = true,
        .needs_additional_support = requires_installation_,
        .confidence = detection.value("confidence").toInt(0) + (requires_installation_ ? 0 : 20),
        .message = detection.value("message").toString().toStdString()
    };
}

StorageProviderCapabilities ExternalProcessStorageProvider::capabilities() const
{
    if (!cached_capabilities_.has_value()) {
        cached_capabilities_ = fetch_capabilities().value_or(StorageProviderCapabilities{});
    }
    return *cached_capabilities_;
}

std::vector<FileEntry> ExternalProcessStorageProvider::list_directory(const std::string& directory,
                                                                      FileScanOptions options) const
{
    QJsonObject request = make_request(manifest_, provider_id_, "list_directory");
    request["directory"] = QString::fromStdString(directory);
    request["options"] = static_cast<int>(options);

    QJsonObject response;
    std::string error;
    if (!invoke_process(manifest_, request, &response, &error)) {
        return {};
    }

    return entries_from_json(response.value("entries").toArray());
}

StoragePathStatus ExternalProcessStorageProvider::inspect_path(const std::string& path) const
{
    QJsonObject request = make_request(manifest_, provider_id_, "inspect_path");
    request["path"] = QString::fromStdString(path);

    QJsonObject response;
    std::string error;
    if (!invoke_process(manifest_, request, &response, &error)) {
        return {};
    }

    return status_from_json(response.value("status").toObject());
}

StorageMovePreflight ExternalProcessStorageProvider::preflight_move(const std::string& source,
                                                                    const std::string& destination) const
{
    QJsonObject request = make_request(manifest_, provider_id_, "preflight_move");
    request["source"] = QString::fromStdString(source);
    request["destination"] = QString::fromStdString(destination);

    QJsonObject response;
    std::string error;
    if (!invoke_process(manifest_, request, &response, &error)) {
        return StorageMovePreflight{
            .allowed = false,
            .message = error
        };
    }

    return preflight_from_json(response.value("preflight").toObject());
}

bool ExternalProcessStorageProvider::path_exists(const std::string& path) const
{
    QJsonObject request = make_request(manifest_, provider_id_, "path_exists");
    request["path"] = QString::fromStdString(path);

    QJsonObject response;
    std::string error;
    return invoke_process(manifest_, request, &response, &error) && response.value("exists").toBool(false);
}

bool ExternalProcessStorageProvider::ensure_directory(const std::string& directory, std::string* error) const
{
    QJsonObject request = make_request(manifest_, provider_id_, "ensure_directory");
    request["directory"] = QString::fromStdString(directory);

    QJsonObject response;
    std::string local_error;
    const bool ok = invoke_process(manifest_, request, &response, &local_error);
    if (!ok && error) {
        *error = local_error;
    }
    return ok;
}

StorageMutationResult ExternalProcessStorageProvider::move_entry(const std::string& source,
                                                                 const std::string& destination) const
{
    QJsonObject request = make_request(manifest_, provider_id_, "move_entry");
    request["source"] = QString::fromStdString(source);
    request["destination"] = QString::fromStdString(destination);

    QJsonObject response;
    std::string error;
    if (!invoke_process(manifest_, request, &response, &error)) {
        return StorageMutationResult{
            .success = false,
            .skipped = false,
            .message = error
        };
    }

    return mutation_from_json(response);
}

StorageMutationResult ExternalProcessStorageProvider::undo_move(const std::string& source,
                                                                const std::string& destination) const
{
    QJsonObject request = make_request(manifest_, provider_id_, "undo_move");
    request["source"] = QString::fromStdString(source);
    request["destination"] = QString::fromStdString(destination);

    QJsonObject response;
    std::string error;
    if (!invoke_process(manifest_, request, &response, &error)) {
        return StorageMutationResult{
            .success = false,
            .skipped = false,
            .message = error
        };
    }

    return mutation_from_json(response);
}

std::optional<StorageProviderCapabilities> ExternalProcessStorageProvider::fetch_capabilities() const
{
    QJsonObject request = make_request(manifest_, provider_id_, "capabilities");

    QJsonObject response;
    std::string error;
    if (!invoke_process(manifest_, request, &response, &error)) {
        return std::nullopt;
    }

    return capabilities_from_json(response.value("capabilities").toObject());
}
