#ifndef ANALYSIS_RUNTIME_LOCK_HPP
#define ANALYSIS_RUNTIME_LOCK_HPP

#include <cstdint>
#include <filesystem>
#include <optional>
#include <memory>
#include <string>

class QLockFile;

/**
 * @brief Coordinates the single active analysis job shared by GUI and headless entry points.
 *
 * The lock uses an OS-visible lock file plus a JSON metadata sidecar. It is intended
 * to protect LLM/runtime/cache/file-mutation work across the GUI app, Explorer worker,
 * and future headless commands.
 */
class AnalysisRuntimeLock {
public:
    /**
     * @brief Identifies the process family holding the runtime lock.
     */
    enum class Owner {
        Unknown,
        Gui,
        ExplorerWorker,
        Headless
    };

    /**
     * @brief Metadata persisted beside the lock file for user-facing status and stale recovery.
     */
    struct Metadata {
        Owner owner{Owner::Unknown};
        std::int64_t pid{0};
        std::string job_id;
        std::string started_at_utc;
        std::string description;
    };

    /**
     * @brief RAII handle for a successfully acquired runtime lock.
     */
    class Lease {
    public:
        /**
         * @brief Destroy the lease and release the lock when still owned.
         */
        ~Lease();

        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;

        /**
         * @brief Move-construct a lease, transferring ownership.
         * @param other Lease to move from.
         */
        Lease(Lease&& other) noexcept;
        /**
         * @brief Move-assign a lease, releasing any current ownership first.
         * @param other Lease to move from.
         * @return This lease.
         */
        Lease& operator=(Lease&& other) noexcept;

        /**
         * @brief Release the lock and remove metadata immediately.
         */
        void release();
        /**
         * @brief Returns whether this lease currently owns the lock.
         * @return True when the lock is held by this lease.
         */
        bool owns_lock() const noexcept { return owns_lock_; }
        /**
         * @brief Returns the metadata written for this lease.
         * @return Lock metadata.
         */
        const Metadata& metadata() const noexcept { return metadata_; }

    private:
        friend class AnalysisRuntimeLock;

        /**
         * @brief Construct an owning lease.
         * @param lock_file Locked QLockFile instance.
         * @param metadata_path Metadata sidecar path.
         * @param metadata Metadata written for this lock.
         */
        Lease(std::unique_ptr<QLockFile> lock_file,
              std::filesystem::path metadata_path,
              Metadata metadata);

        std::unique_ptr<QLockFile> lock_file_;
        std::filesystem::path metadata_path_;
        Metadata metadata_;
        bool owns_lock_{false};
    };

    /**
     * @brief Create a runtime lock using a directory for lock state.
     * @param runtime_dir Directory where lock and metadata files are stored.
     */
    explicit AnalysisRuntimeLock(std::filesystem::path runtime_dir);

    /**
     * @brief Try to acquire the shared runtime lock.
     * @param metadata Metadata to write if acquisition succeeds. Missing PID/start time are filled in.
     * @param error Optional output for a diagnostic failure message.
     * @return Owning lease when acquired, otherwise std::nullopt.
     */
    std::optional<Lease> try_acquire(Metadata metadata, std::string* error = nullptr) const;
    /**
     * @brief Returns whether another process or lease currently holds the lock.
     * @param metadata Optional output for persisted lock metadata.
     * @return True when the lock is currently held.
     */
    bool is_locked(Metadata* metadata = nullptr) const;
    /**
     * @brief Read the current metadata sidecar without attempting to acquire the lock.
     * @return Metadata when a valid sidecar exists.
     */
    std::optional<Metadata> read_metadata() const;

    /**
     * @brief Converts an owner enum to its persisted string value.
     * @param owner Owner enum value.
     * @return Stable owner string.
     */
    static std::string owner_to_string(Owner owner);
    /**
     * @brief Converts a persisted owner string to an enum value.
     * @param value Owner string.
     * @return Owner enum value.
     */
    static Owner owner_from_string(const std::string& value);

    /**
     * @brief Returns the lock file path.
     * @return Absolute or relative lock file path.
     */
    std::filesystem::path lock_file_path() const { return lock_path_; }
    /**
     * @brief Returns the metadata sidecar path.
     * @return Absolute or relative metadata file path.
     */
    std::filesystem::path metadata_file_path() const { return metadata_path_; }

private:
    std::filesystem::path runtime_dir_;
    std::filesystem::path lock_path_;
    std::filesystem::path metadata_path_;
};

#endif // ANALYSIS_RUNTIME_LOCK_HPP
