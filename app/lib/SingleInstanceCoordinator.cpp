#include "SingleInstanceCoordinator.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>

namespace {

QString normalized_instance_id(QString instance_id)
{
    instance_id = instance_id.trimmed();
    if (instance_id.isEmpty()) {
        instance_id = QStringLiteral("net.quicknode.AIFileSorter");
    }
    return instance_id;
}

QString runtime_directory()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (path.isEmpty()) {
        path = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }
    if (path.isEmpty()) {
        path = QDir::tempPath();
    }
    return path;
}

} // namespace

SingleInstanceCoordinator::SingleInstanceCoordinator(QString instance_id)
    : instance_id_(normalized_instance_id(std::move(instance_id))),
      server_name_(build_server_name(instance_id_)),
      lock_file_path_(build_lock_file_path(instance_id_)),
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
    activation_callback_ = std::move(callback);
}

QString SingleInstanceCoordinator::build_server_name(const QString& instance_id)
{
    const QByteArray digest = QCryptographicHash::hash(
        normalized_instance_id(instance_id).toUtf8(),
        QCryptographicHash::Sha256).toHex();
    return QStringLiteral("net.quicknode.AIFileSorter.instance.%1")
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
    QLocalSocket socket;
    socket.connectToServer(server_name_, QIODevice::WriteOnly);
    if (!socket.waitForConnected(300)) {
        return false;
    }

    socket.flush();
    socket.waitForBytesWritten(100);
    socket.disconnectFromServer();
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

    if (notified && activation_callback_) {
        activation_callback_();
    }
}
