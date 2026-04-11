/**
 * @file SingleInstanceCoordinator.hpp
 * @brief Coordinates single-instance enforcement and activation handoff.
 */
#pragma once

#include <QString>

#include <functional>
#include <memory>

class QLocalServer;
class QLockFile;

/**
 * @brief Enforces a single running GUI instance and forwards second-launch activation requests.
 *
 * The primary instance acquires a per-user lock file and listens on a local IPC endpoint.
 * Later launches notify the primary instance through that endpoint and then exit cleanly.
 */
class SingleInstanceCoordinator
{
public:
    /**
     * @brief Create a coordinator for the given logical application instance id.
     * @param instance_id Stable identifier shared by all launches that should count as one app.
     */
    explicit SingleInstanceCoordinator(QString instance_id);

    /**
     * @brief Releases the local listener and lock file, if held.
     */
    ~SingleInstanceCoordinator();

    SingleInstanceCoordinator(const SingleInstanceCoordinator&) = delete;
    SingleInstanceCoordinator& operator=(const SingleInstanceCoordinator&) = delete;

    /**
     * @brief Try to become the primary instance or notify the already running one.
     * @return True when this process should continue startup, false when it should exit.
     */
    bool acquire_primary_instance();

    /**
     * @brief Report whether this process owns the primary-instance lock.
     * @return True when this process is the primary instance.
     */
    bool is_primary_instance() const noexcept;

    /**
     * @brief Set the callback invoked when a secondary launch asks the primary instance to activate.
     * @param callback Function called on the primary instance after a secondary launch connects.
     */
    void set_activation_callback(std::function<void()> callback);

    /**
     * @brief Build a stable local server name for a logical instance id.
     * @param instance_id Stable identifier shared by cooperating launches.
     * @return IPC-safe server name derived from the instance id.
     */
    static QString build_server_name(const QString& instance_id);

private:
    /**
     * @brief Build the lock-file path used to enforce single-instance ownership.
     * @param instance_id Stable identifier shared by cooperating launches.
     * @return Absolute path to the coordinator lock file.
     */
    static QString build_lock_file_path(const QString& instance_id);

    /**
     * @brief Start listening for activation notifications from later launches.
     * @return True when the local server is ready.
     */
    bool start_primary_listener();

    /**
     * @brief Notify the already running primary instance that it should activate its window.
     * @return True when the notification reached an existing primary instance.
     */
    bool notify_primary_instance() const;

    /**
     * @brief Drain pending activation connections and invoke the activation callback.
     */
    void handle_activation_requests();

    QString instance_id_;
    QString server_name_;
    QString lock_file_path_;
    std::unique_ptr<QLockFile> lock_file_;
    std::unique_ptr<QLocalServer> server_;
    std::function<void()> activation_callback_;
    bool primary_instance_{false};
};
