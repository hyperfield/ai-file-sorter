#pragma once

#include <spdlog/sinks/base_sink.h>
#include <filesystem>
#include <fstream>
#include <mutex>

/** @brief Rotating log sink using native filesystem streams without altering spdlog's filename ABI. */
class NativeRotatingFileSink final : public spdlog::sinks::base_sink<std::mutex> {
public:
    /**
     * @brief Open an append-only log and retain numbered rotation files.
     * @param path Native log filename.
     * @param max_bytes Maximum active file size before rotation.
     * @param max_files Number of rotated files retained.
     */
    NativeRotatingFileSink(std::filesystem::path path, std::size_t max_bytes, std::size_t max_files);
protected:
    /** @brief Format and append a log record. @param message Record to write. */
    void sink_it_(const spdlog::details::log_msg& message) override;
    /** @brief Flush the active native file stream. */
    void flush_() override;
private:
    /** @brief Open the active log file. */
    void open();
    /** @brief Rotate the active log and numbered backups. */
    void rotate();
    /** @brief Return a numbered backup filename. @param index Backup number, starting at one. */
    std::filesystem::path backup_path(std::size_t index) const;
    std::filesystem::path path_;
    std::ofstream stream_;
    std::size_t max_bytes_;
    std::size_t max_files_;
    std::size_t size_{0};
};
