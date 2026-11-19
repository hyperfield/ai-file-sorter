#include "NativeRotatingFileSink.hpp"
#include "Utils.hpp"
#include <spdlog/common.h>

NativeRotatingFileSink::NativeRotatingFileSink(std::filesystem::path path,
    std::size_t max_bytes, std::size_t max_files)
    : path_(std::move(path)), max_bytes_(max_bytes), max_files_(max_files)
{
    if (max_bytes == 0 || max_files == 0) throw spdlog::spdlog_ex("Invalid log rotation limits");
    if (!path_.parent_path().empty()) std::filesystem::create_directories(path_.parent_path());
    open();
}

void NativeRotatingFileSink::open()
{
    stream_.open(path_, std::ios::binary | std::ios::app);
    if (!stream_) throw spdlog::spdlog_ex("Cannot open log: " + Utils::path_to_utf8(path_));
    size_ = static_cast<std::size_t>(std::filesystem::file_size(path_));
}

std::filesystem::path NativeRotatingFileSink::backup_path(std::size_t index) const
{
    return path_.parent_path() / Utils::utf8_to_path(Utils::path_to_utf8(path_.stem()) + "." +
        std::to_string(index) + Utils::path_to_utf8(path_.extension()));
}

void NativeRotatingFileSink::rotate()
{
    stream_.close();
    for (auto index = max_files_; index > 0; --index) {
        const auto source = index == 1 ? path_ : backup_path(index - 1);
        const auto target = backup_path(index);
        std::filesystem::remove(target);
        if (std::filesystem::exists(source)) std::filesystem::rename(source, target);
    }
    open();
}

void NativeRotatingFileSink::sink_it_(const spdlog::details::log_msg& message)
{
    spdlog::memory_buf_t formatted;
    formatter_->format(message, formatted);
    if (size_ > 0 && size_ + formatted.size() > max_bytes_) rotate();
    stream_.write(formatted.data(), static_cast<std::streamsize>(formatted.size()));
    if (!stream_) throw spdlog::spdlog_ex("Cannot write log: " + Utils::path_to_utf8(path_));
    size_ += formatted.size();
}

void NativeRotatingFileSink::flush_()
{
    stream_.flush();
    if (!stream_) throw spdlog::spdlog_ex("Cannot flush log: " + Utils::path_to_utf8(path_));
}
