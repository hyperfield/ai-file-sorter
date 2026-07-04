#include "AnalysisRuntimeLock.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLockFile>
#include <QSaveFile>
#include <QString>
#include <QSysInfo>

#include <cerrno>
#include <system_error>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#endif

namespace {

QString to_qstring(const std::filesystem::path& path)
{
#ifdef _WIN32
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}

std::int64_t current_process_id()
{
    return static_cast<std::int64_t>(QCoreApplication::applicationPid());
}

std::string utc_now_iso()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();
}

bool process_is_running(std::int64_t pid)
{
    if (pid <= 0) {
        return false;
    }
#ifdef _WIN32
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!process) {
        return GetLastError() == ERROR_ACCESS_DENIED;
    }
    DWORD exit_code = 0;
    const bool running = GetExitCodeProcess(process, &exit_code) && exit_code == STILL_ACTIVE;
    CloseHandle(process);
    return running;
#else
    if (::kill(static_cast<pid_t>(pid), 0) == 0) {
        return true;
    }
    return errno == EPERM;
#endif
}

bool same_host(const QString& lock_hostname)
{
    return lock_hostname.isEmpty() ||
           lock_hostname.compare(QSysInfo::machineHostName(), Qt::CaseInsensitive) == 0;
}

QJsonObject metadata_to_json(const AnalysisRuntimeLock::Metadata& metadata)
{
    QJsonObject object;
    object.insert(QStringLiteral("owner"),
                  QString::fromStdString(AnalysisRuntimeLock::owner_to_string(metadata.owner)));
    object.insert(QStringLiteral("pid"), QString::number(metadata.pid));
    object.insert(QStringLiteral("jobId"), QString::fromStdString(metadata.job_id));
    object.insert(QStringLiteral("startedAtUtc"), QString::fromStdString(metadata.started_at_utc));
    object.insert(QStringLiteral("description"), QString::fromStdString(metadata.description));
    return object;
}

AnalysisRuntimeLock::Metadata metadata_from_json(const QJsonObject& object)
{
    AnalysisRuntimeLock::Metadata metadata;
    metadata.owner = AnalysisRuntimeLock::owner_from_string(
        object.value(QStringLiteral("owner")).toString().toStdString());
    const QJsonValue pid_value = object.value(QStringLiteral("pid"));
    metadata.pid = pid_value.isDouble()
                       ? static_cast<std::int64_t>(pid_value.toDouble())
                       : pid_value.toString().toLongLong();
    metadata.job_id = object.value(QStringLiteral("jobId")).toString().toStdString();
    metadata.started_at_utc = object.value(QStringLiteral("startedAtUtc")).toString().toStdString();
    metadata.description = object.value(QStringLiteral("description")).toString().toStdString();
    return metadata;
}

bool write_metadata(const std::filesystem::path& path,
                    const AnalysisRuntimeLock::Metadata& metadata,
                    std::string* error)
{
    QSaveFile file(to_qstring(path));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = file.errorString().toStdString();
        }
        return false;
    }

    const QByteArray payload = QJsonDocument(metadata_to_json(metadata)).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size()) {
        if (error) {
            *error = file.errorString().toStdString();
        }
        return false;
    }
    if (!file.commit()) {
        if (error) {
            *error = file.errorString().toStdString();
        }
        return false;
    }
    return true;
}

bool remove_dead_owner_lock(QLockFile& lock_file,
                            const std::filesystem::path& lock_path,
                            const std::filesystem::path& metadata_path)
{
    qint64 pid = 0;
    QString hostname;
    QString appname;
    if (!lock_file.getLockInfo(&pid, &hostname, &appname)) {
        return false;
    }
    if (!same_host(hostname) || process_is_running(pid)) {
        return false;
    }

    QFile::remove(to_qstring(metadata_path));
    if (lock_file.removeStaleLockFile()) {
        return true;
    }
    return QFile::remove(to_qstring(lock_path));
}

std::string lock_info_message(QLockFile& lock_file)
{
    qint64 pid = 0;
    QString hostname;
    QString appname;
    if (!lock_file.getLockInfo(&pid, &hostname, &appname)) {
        return "Analysis is already running.";
    }
    return QStringLiteral("Analysis is already running (pid %1, host %2).")
        .arg(pid)
        .arg(hostname)
        .toStdString();
}

} // namespace

AnalysisRuntimeLock::Lease::Lease(std::unique_ptr<QLockFile> lock_file,
                                  std::filesystem::path metadata_path,
                                  Metadata metadata)
    : lock_file_(std::move(lock_file)),
      metadata_path_(std::move(metadata_path)),
      metadata_(std::move(metadata)),
      owns_lock_(true)
{
}

AnalysisRuntimeLock::Lease::~Lease()
{
    release();
}

AnalysisRuntimeLock::Lease::Lease(Lease&& other) noexcept
    : lock_file_(std::move(other.lock_file_)),
      metadata_path_(std::move(other.metadata_path_)),
      metadata_(std::move(other.metadata_)),
      owns_lock_(other.owns_lock_)
{
    other.owns_lock_ = false;
}

AnalysisRuntimeLock::Lease& AnalysisRuntimeLock::Lease::operator=(Lease&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    release();
    lock_file_ = std::move(other.lock_file_);
    metadata_path_ = std::move(other.metadata_path_);
    metadata_ = std::move(other.metadata_);
    owns_lock_ = other.owns_lock_;
    other.owns_lock_ = false;
    return *this;
}

void AnalysisRuntimeLock::Lease::release()
{
    if (!owns_lock_) {
        return;
    }
    QFile::remove(to_qstring(metadata_path_));
    if (lock_file_ && lock_file_->isLocked()) {
        lock_file_->unlock();
    }
    owns_lock_ = false;
}

AnalysisRuntimeLock::AnalysisRuntimeLock(std::filesystem::path runtime_dir)
    : runtime_dir_(std::move(runtime_dir)),
      lock_path_(runtime_dir_ / "analysis-runtime.lock"),
      metadata_path_(runtime_dir_ / "analysis-runtime.lock.json")
{
}

std::optional<AnalysisRuntimeLock::Lease>
AnalysisRuntimeLock::try_acquire(Metadata metadata, std::string* error) const
{
    std::error_code ec;
    std::filesystem::create_directories(runtime_dir_, ec);
    if (ec) {
        if (error) {
            *error = ec.message();
        }
        return std::nullopt;
    }

    auto lock_file = std::make_unique<QLockFile>(to_qstring(lock_path_));
    lock_file->setStaleLockTime(0);

    if (!lock_file->tryLock(0)) {
        if (remove_dead_owner_lock(*lock_file, lock_path_, metadata_path_)) {
            lock_file = std::make_unique<QLockFile>(to_qstring(lock_path_));
            lock_file->setStaleLockTime(0);
        }
    }

    if (!lock_file->isLocked() && !lock_file->tryLock(0)) {
        if (error) {
            *error = lock_info_message(*lock_file);
        }
        return std::nullopt;
    }

    if (metadata.pid <= 0) {
        metadata.pid = current_process_id();
    }
    if (metadata.started_at_utc.empty()) {
        metadata.started_at_utc = utc_now_iso();
    }

    std::string metadata_error;
    if (!write_metadata(metadata_path_, metadata, &metadata_error)) {
        if (error) {
            *error = "Could not write analysis runtime metadata: " + metadata_error;
        }
        lock_file->unlock();
        return std::nullopt;
    }

    return Lease(std::move(lock_file), metadata_path_, std::move(metadata));
}

bool AnalysisRuntimeLock::is_locked(Metadata* metadata) const
{
    QLockFile lock_file(to_qstring(lock_path_));
    lock_file.setStaleLockTime(0);
    if (lock_file.tryLock(0)) {
        QFile::remove(to_qstring(metadata_path_));
        lock_file.unlock();
        return false;
    }

    if (remove_dead_owner_lock(lock_file, lock_path_, metadata_path_)) {
        QLockFile retry(to_qstring(lock_path_));
        retry.setStaleLockTime(0);
        if (retry.tryLock(0)) {
            retry.unlock();
            return false;
        }
    }

    if (metadata) {
        if (auto stored = read_metadata()) {
            *metadata = *stored;
        } else {
            qint64 pid = 0;
            QString hostname;
            QString appname;
            if (lock_file.getLockInfo(&pid, &hostname, &appname)) {
                metadata->pid = pid;
                metadata->owner = Owner::Unknown;
                metadata->description = appname.toStdString();
            }
        }
    }
    return true;
}

std::optional<AnalysisRuntimeLock::Metadata> AnalysisRuntimeLock::read_metadata() const
{
    QFile file(to_qstring(metadata_path_));
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return std::nullopt;
    }
    return metadata_from_json(doc.object());
}

std::string AnalysisRuntimeLock::owner_to_string(Owner owner)
{
    switch (owner) {
    case Owner::Gui:
        return "gui";
    case Owner::ExplorerWorker:
        return "explorerWorker";
    case Owner::Headless:
        return "headless";
    case Owner::Unknown:
    default:
        return "unknown";
    }
}

AnalysisRuntimeLock::Owner AnalysisRuntimeLock::owner_from_string(const std::string& value)
{
    if (value == "gui") {
        return Owner::Gui;
    }
    if (value == "explorerWorker") {
        return Owner::ExplorerWorker;
    }
    if (value == "headless") {
        return Owner::Headless;
    }
    return Owner::Unknown;
}
