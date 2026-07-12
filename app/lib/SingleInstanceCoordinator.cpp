#include "SingleInstanceCoordinator.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QtNetwork/QLocalServer>
#include <QtNetwork/QLocalSocket>
#include <QStandardPaths>

#include <cstdlib>

namespace {

QString normalized_instance_id(QString instance_id)
{
    instance_id = instance_id.trimmed();
    if (instance_id.isEmpty()) {
        instance_id = QStringLiteral("dev.hfstudio.AIFileSorter");
    }
    return instance_id;
}

QString runtime_directory()
{
    if (const char* override_dir = std::getenv("AI_FILE_SORTER_SINGLE_INSTANCE_RUNTIME_DIR");
        override_dir && *override_dir) {
        QDir dir(QString::fromLocal8Bit(override_dir));
        dir.mkpath(QStringLiteral("."));
        return dir.absolutePath();
    }

    QString path = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (path.isEmpty()) {
        path = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }
    if (path.isEmpty()) {
        path = QDir::tempPath();
    }
    return path;
}

QString build_server_endpoint(const QString& server_name)
{
#ifndef _WIN32
    if (const char* override_dir = std::getenv("AI_FILE_SORTER_SINGLE_INSTANCE_RUNTIME_DIR");
        override_dir && *override_dir) {
        QDir dir(QString::fromLocal8Bit(override_dir));
        dir.mkpath(QStringLiteral("."));
        const QByteArray digest = QCryptographicHash::hash(server_name.toUtf8(),
                                                           QCryptographicHash::Sha256).toHex();
        return dir.filePath(QStringLiteral("aifs-%1.sock")
                                .arg(QString::fromLatin1(digest.left(32))));
    }
#endif
    return server_name;
}

} // namespace

SingleInstanceCoordinator::SingleInstanceCoordinator(QString instance_id)
    : instance_id_(normalized_instance_id(std::move(instance_id))),
      server_name_(build_server_endpoint(build_server_name(instance_id_))),
      lock_file_path_(build_lock_file_path(instance_id_)),
      activation_message_path_(lock_file_path_ + QStringLiteral(".activation")),
      lock_file_(std::make_unique<QLockFile>(lock_file_path_))
{
}

SingleInstanceCoordinator::~SingleInstanceCoordinator()
{
    if (server_) {
        server_->close();
        QLocalServer::removeServer(server_name_);
    }
    if (lock_file_ && lock_file_->isLocked()) {
        lock_file_->unlock();
    }
}

bool SingleInstanceCoordinator::acquire_primary_instance()
{
    if (!lock_file_) {
        return true;
    }

    if (!lock_file_->tryLock(0)) {
        primary_instance_ = false;
        if (!notify_primary_instance()) {
            qWarning().noquote()
                << "Another AI File Sorter instance appears to be running,"
                << "but the activation request could not be delivered.";
        }
        return false;
    }

    primary_instance_ = true;
    if (!start_primary_listener()) {
        qWarning().noquote()
            << "Single-instance listener could not be started; continuing with lock-only protection.";
    }
    return true;
}

bool SingleInstanceCoordinator::is_primary_instance() const noexcept
{
    return primary_instance_;
}

void SingleInstanceCoordinator::set_activation_callback(std::function<void()> callback)
{
    activation_callback_ = [callback = std::move(callback)](const QString&) {
        if (callback) {
            callback();
        }
    };
}

void SingleInstanceCoordinator::set_activation_callback(std::function<void(QString)> callback)
{
    activation_callback_ = std::move(callback);
}

void SingleInstanceCoordinator::set_activation_message(QString message)
{
    activation_message_ = std::move(message);
}

QString SingleInstanceCoordinator::build_server_name(const QString& instance_id)
{
    const QByteArray digest = QCryptographicHash::hash(
        normalized_instance_id(instance_id).toUtf8(),
        QCryptographicHash::Sha256).toHex();
    return QStringLiteral("dev.hfstudio.AIFileSorter.instance.%1")
        .arg(QString::fromLatin1(digest.left(32)));
}

QString SingleInstanceCoordinator::build_lock_file_path(const QString& instance_id)
{
    const QString base_dir = runtime_directory();
    QDir dir(base_dir);
    dir.mkpath(QStringLiteral("."));
    return dir.filePath(build_server_name(instance_id) + QStringLiteral(".lock"));
}

bool SingleInstanceCoordinator::start_primary_listener()
{
    server_ = std::make_unique<QLocalServer>();
    QObject::connect(server_.get(),
                     &QLocalServer::newConnection,
                     server_.get(),
                     [this]() { handle_activation_requests(); });

    if (server_->listen(server_name_)) {
        return true;
    }

    if (server_->serverError() == QAbstractSocket::AddressInUseError) {
        QLocalServer::removeServer(server_name_);
        if (server_->listen(server_name_)) {
            return true;
        }
    }

    qWarning().noquote()
        << "Failed to listen for single-instance activation on"
        << server_name_
        << ":"
        << server_->errorString();
    server_.reset();
    return false;
}

bool SingleInstanceCoordinator::notify_primary_instance() const
{
    if (!activation_message_.isEmpty()) {
        QFile message_file(activation_message_path_);
        if (message_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            message_file.write(activation_message_.toUtf8());
            message_file.close();
        }
    }

    QLocalSocket socket;
    socket.connectToServer(server_name_);
    if (!socket.waitForConnected(300)) {
        if (!activation_message_.isEmpty()) {
            QFile::remove(activation_message_path_);
        }
        return false;
    }

    socket.flush();
    socket.disconnectFromServer();
    socket.waitForDisconnected(100);
    return true;
}

void SingleInstanceCoordinator::handle_activation_requests()
{
    if (!server_) {
        return;
    }

    bool notified = false;
    while (QLocalSocket* socket = server_->nextPendingConnection()) {
        notified = true;
        socket->disconnectFromServer();
        socket->deleteLater();
    }

    if (!notified || !activation_callback_) {
        return;
    }

    QString message;
    QFile message_file(activation_message_path_);
    if (message_file.open(QIODevice::ReadOnly)) {
        message = QString::fromUtf8(message_file.readAll());
        message_file.close();
        QFile::remove(activation_message_path_);
    }
    activation_callback_(message);
}
