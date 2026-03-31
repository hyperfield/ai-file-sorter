#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

constexpr int kOptionFiles = 1;
constexpr int kOptionDirectories = 2;
constexpr int kOptionHiddenFiles = 4;
constexpr int kOptionRecursive = 8;

std::string file_type_name(const std::filesystem::directory_entry& entry)
{
    return entry.is_directory() ? "Directory" : "File";
}

bool should_include(const std::filesystem::directory_entry& entry, int options)
{
    const bool is_hidden = entry.path().filename().string().starts_with(".");
    if (is_hidden && (options & kOptionHiddenFiles) == 0) {
        return false;
    }

    if (entry.is_regular_file()) {
        return (options & kOptionFiles) != 0;
    }
    if (entry.is_directory()) {
        return (options & kOptionDirectories) != 0;
    }
    return false;
}

std::time_t read_mtime(const std::filesystem::path& path)
{
    std::error_code ec;
    const auto write_time = std::filesystem::last_write_time(path, ec);
    if (ec) {
        return 0;
    }

    const auto delta = write_time - std::filesystem::file_time_type::clock::now();
    const auto system_time = std::chrono::system_clock::now() +
        std::chrono::duration_cast<std::chrono::system_clock::duration>(delta);
    return std::chrono::system_clock::to_time_t(system_time);
}

QJsonArray list_entries(const std::filesystem::path& root, int options)
{
    QJsonArray entries;
    std::error_code ec;
    if ((options & kOptionRecursive) != 0) {
        for (std::filesystem::recursive_directory_iterator it(root, ec), end; it != end && !ec; it.increment(ec)) {
            if (!should_include(*it, options)) {
                continue;
            }
            QJsonObject entry;
            entry["full_path"] = QString::fromStdString(it->path().string());
            entry["file_name"] = QString::fromStdString(it->path().filename().string());
            entry["type"] = QString::fromStdString(file_type_name(*it));
            entries.append(entry);
        }
        return entries;
    }

    for (std::filesystem::directory_iterator it(root, ec), end; it != end && !ec; it.increment(ec)) {
        if (!should_include(*it, options)) {
            continue;
        }
        QJsonObject entry;
        entry["full_path"] = QString::fromStdString(it->path().string());
        entry["file_name"] = QString::fromStdString(it->path().filename().string());
        entry["type"] = QString::fromStdString(file_type_name(*it));
        entries.append(entry);
    }
    return entries;
}

QJsonObject mutation_result(bool success,
                            bool skipped,
                            const std::string& message,
                            const std::filesystem::path& path = {})
{
    QJsonObject result;
    result["success"] = true;
    result["mutation_success"] = success;
    result["skipped"] = skipped;
    result["message"] = QString::fromStdString(message);

    QJsonObject metadata;
    if (success && !path.empty()) {
        std::error_code ec;
        qint64 size = static_cast<qint64>(std::filesystem::file_size(path, ec));
        if (ec) {
            size = 0;
        }
        metadata["size_bytes"] = size;
        const qint64 mtime = static_cast<qint64>(read_mtime(path));
        metadata["mtime"] = mtime;
        metadata["stable_identity"] = QString::fromStdString(path.lexically_normal().string());
        metadata["revision_token"] = QStringLiteral("%1:%2")
            .arg(size)
            .arg(mtime);
    }
    result["metadata"] = metadata;
    return result;
}

QJsonObject path_status(const std::filesystem::path& path)
{
    std::error_code ec;
    QJsonObject status;
    const bool exists = std::filesystem::exists(path, ec) && !ec;
    status["exists"] = exists;
    status["hydration_required"] = false;
    status["sync_locked"] = path.filename().string().starts_with("~$");
    status["conflict_copy"] = false;
    status["should_retry"] = status["sync_locked"].toBool(false);
    status["retry_after_ms"] = status["sync_locked"].toBool(false) ? 2000 : 0;
    status["stable_identity"] = QString::fromStdString(path.lexically_normal().string());
    if (exists) {
        const qint64 size = static_cast<qint64>(std::filesystem::file_size(path, ec));
        const qint64 mtime = static_cast<qint64>(read_mtime(path));
        status["revision_token"] = QStringLiteral("%1:%2").arg(size).arg(mtime);
    } else {
        status["revision_token"] = QString();
    }
    status["message"] = status["sync_locked"].toBool(false)
        ? QStringLiteral("MockCloud file is still locked.")
        : QString();
    return status;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const auto payload = std::string(std::istreambuf_iterator<char>(std::cin),
                                     std::istreambuf_iterator<char>());
    const QJsonDocument request_doc = QJsonDocument::fromJson(QByteArray::fromStdString(payload));
    if (!request_doc.isObject()) {
        std::cerr << "{\"success\":false,\"error\":\"invalid request\"}";
        return 1;
    }

    const QJsonObject request = request_doc.object();
    const QString action = request.value("action").toString();
    const QString provider_id = request.value("provider_id").toString();

    QJsonObject response;
    response["success"] = true;

    if (action == QStringLiteral("probe")) {
        QJsonArray provider_ids;
        provider_ids.append(QStringLiteral("mockcloud"));
        response["provider_ids"] = provider_ids;
    } else if (action == QStringLiteral("detect")) {
        const QString root_path = request.value("root_path").toString();
        QJsonObject detection;
        const bool matched =
            provider_id == QStringLiteral("mockcloud") &&
            root_path.contains(QStringLiteral("MockCloud"), Qt::CaseInsensitive);
        detection["matched"] = matched;
        detection["confidence"] = matched ? 65 : 0;
        detection["message"] = matched
            ? QStringLiteral("Detected a MockCloud folder. External compatibility support is available.")
            : QString();
        response["detection"] = detection;
    } else if (action == QStringLiteral("capabilities")) {
        QJsonObject capabilities;
        capabilities["supports_online_only_files"] = true;
        capabilities["supports_atomic_rename"] = false;
        capabilities["should_skip_reparse_points"] = true;
        capabilities["should_relax_undo_mtime_validation"] = true;
        response["capabilities"] = capabilities;
    } else if (action == QStringLiteral("list_directory")) {
        const std::filesystem::path root = request.value("directory").toString().toStdString();
        const int options = request.value("options").toInt();
        response["entries"] = list_entries(root, options);
    } else if (action == QStringLiteral("inspect_path")) {
        response["status"] = path_status(request.value("path").toString().toStdString());
    } else if (action == QStringLiteral("preflight_move")) {
        const auto source = path_status(request.value("source").toString().toStdString());
        const auto destination = path_status(request.value("destination").toString().toStdString());
        QJsonObject preflight;
        const bool source_exists = source.value("exists").toBool(false);
        const bool destination_exists = destination.value("exists").toBool(false);
        const bool sync_locked = source.value("sync_locked").toBool(false);
        preflight["allowed"] = source_exists && !destination_exists && !sync_locked;
        preflight["skipped"] = !source_exists || destination_exists;
        preflight["hydration_required"] = false;
        preflight["sync_locked"] = sync_locked;
        preflight["destination_conflict"] = destination_exists;
        preflight["should_retry"] = sync_locked;
        preflight["retry_after_ms"] = sync_locked ? 2000 : 0;
        preflight["source_status"] = source;
        preflight["destination_status"] = destination;
        if (!source_exists) {
            preflight["message"] = QStringLiteral("Source path is missing.");
        } else if (destination_exists) {
            preflight["message"] = QStringLiteral("Destination path already exists.");
        } else if (sync_locked) {
            preflight["message"] = QStringLiteral("MockCloud file is still locked.");
        } else {
            preflight["message"] = QString();
        }
        response["preflight"] = preflight;
    } else if (action == QStringLiteral("path_exists")) {
        std::error_code ec;
        response["exists"] = std::filesystem::exists(
            request.value("path").toString().toStdString(),
            ec);
    } else if (action == QStringLiteral("ensure_directory")) {
        std::error_code ec;
        std::filesystem::create_directories(request.value("directory").toString().toStdString(), ec);
        if (ec) {
            response["success"] = false;
            response["error"] = QString::fromStdString(ec.message());
        }
    } else if (action == QStringLiteral("move_entry") || action == QStringLiteral("undo_move")) {
        auto source = std::filesystem::path(request.value("source").toString().toStdString());
        auto destination = std::filesystem::path(request.value("destination").toString().toStdString());
        if (action == QStringLiteral("undo_move")) {
            std::swap(source, destination);
        }
        std::error_code ec;
        if (!std::filesystem::exists(source, ec) || ec) {
            response = mutation_result(false, true, "Source path is missing.");
        } else if (std::filesystem::exists(destination, ec) && !ec) {
            response = mutation_result(false, true, "Destination path already exists.");
        } else {
            std::filesystem::create_directories(destination.parent_path(), ec);
            if (ec) {
                response["success"] = false;
                response["error"] = QString::fromStdString(ec.message());
            } else {
                std::filesystem::rename(source, destination, ec);
                if (ec) {
                    response["success"] = false;
                    response["error"] = QString::fromStdString(ec.message());
                } else {
                    response = mutation_result(true, false, std::string(), destination);
                }
            }
        }
    } else {
        response["success"] = false;
        response["error"] = QStringLiteral("unsupported action");
    }

    std::cout << QJsonDocument(response).toJson(QJsonDocument::Compact).constData();
    return response.value("success").toBool(false) ? 0 : 1;
}
