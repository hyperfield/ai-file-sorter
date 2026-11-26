#include "LocalFileTransfer.hpp"

#include "Logger.hpp"
#include "Utils.hpp"

#include <QTemporaryDir>
#include <cerrno>
#include <cstdio>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace {
namespace fs = std::filesystem;

// Publication must not overwrite an entry created after the provider's preflight.
void rename_exclusively(const fs::path& source, const fs::path& destination, std::error_code& ec)
{
#ifdef _WIN32
    if (!MoveFileExW(source.c_str(), destination.c_str(), 0)) {
        ec = std::error_code(static_cast<int>(GetLastError()), std::system_category());
        return;
    }
#elif defined(__APPLE__)
    if (renamex_np(source.c_str(), destination.c_str(), RENAME_EXCL) != 0) {
        ec = std::error_code(errno, std::generic_category());
        return;
    }
#elif defined(__linux__)
    if (syscall(SYS_renameat2, AT_FDCWD, source.c_str(), AT_FDCWD, destination.c_str(), RENAME_NOREPLACE) != 0) {
        ec = std::error_code(errno, std::generic_category());
        return;
    }
#else
    ec = std::make_error_code(std::errc::operation_not_supported);
    return;
#endif
    ec.clear();
}

void copy_entry(const fs::path& source, const fs::path& destination, std::error_code& ec);

void copy_directory_contents(const fs::path& source, const fs::path& destination, std::error_code& ec)
{
    fs::create_directory(destination, ec);
    if (ec) return;
    fs::directory_iterator it(source, ec);
    const fs::directory_iterator end;
    while (!ec && it != end) {
        copy_entry(it->path(), destination / it->path().filename(), ec);
        if (ec) return;
        it.increment(ec);
    }
}

void copy_entry(const fs::path& source, const fs::path& destination, std::error_code& ec)
{
    const auto status = fs::symlink_status(source, ec);
    if (ec) return;
    if (fs::is_symlink(status)) {
        fs::copy_symlink(source, destination, ec);
        return;
    }
    if (fs::is_directory(status)) {
        copy_directory_contents(source, destination, ec);
    } else if (fs::is_regular_file(status)) {
        fs::copy_file(source, destination, fs::copy_options::none, ec);
    } else {
        ec = std::make_error_code(std::errc::operation_not_supported);
    }
    if (ec) return;

    const auto write_time = fs::last_write_time(source, ec);
    if (ec) return;
    fs::last_write_time(destination, write_time, ec);
    if (ec) return;
    fs::permissions(destination, status.permissions(), fs::perm_options::replace, ec);
}

void remove_entry(const fs::path& path, std::error_code& ec)
{
    fs::remove_all(path, ec);
}
} // namespace

LocalFileTransfer::LocalFileTransfer(Operations operations)
    : operations_(std::move(operations))
{
    if (!operations_.rename) operations_.rename = rename_exclusively;
    if (!operations_.copy) operations_.copy = copy_entry;
    if (!operations_.remove) operations_.remove = remove_entry;
}

StorageMutationResult LocalFileTransfer::move(const Path& source, const Path& destination) const
{
    std::error_code ec;
    operations_.rename(source, destination, ec);
    if (!ec) return StorageMutationResult{.success = true};
    if (ec != std::errc::cross_device_link) return StorageMutationResult{.message = ec.message()};

    if (const auto logger = Logger::get_logger("core_logger")) {
        logger->info("Moving between filesystems using copy and remove: '{}' -> '{}'",
                     Utils::path_to_utf8(source), Utils::path_to_utf8(destination));
    }
    return copy_and_remove(source, destination);
}

StorageMutationResult LocalFileTransfer::copy_and_remove(const Path& source, const Path& destination) const
{
    QTemporaryDir staging(QString::fromStdString(
        Utils::path_to_utf8(destination.parent_path() / ".aifs-move-XXXXXX")));
    if (!staging.isValid()) {
        return StorageMutationResult{.message = "Failed to create move staging directory: " +
                                                staging.errorString().toStdString()};
    }
    const auto staged_entry = Utils::utf8_to_path(staging.path().toStdString()) / "entry";
    std::error_code ec;
    operations_.copy(source, staged_entry, ec);
    if (ec) return StorageMutationResult{.message = "Failed to copy source: " + ec.message()};

    rename_exclusively(staged_entry, destination, ec);
    if (ec) return StorageMutationResult{.message = "Failed to publish copied entry: " + ec.message()};

    operations_.remove(source, ec);
    if (ec) {
        return StorageMutationResult{
            .message = "Copy completed, but failed to remove source '" + Utils::path_to_utf8(source) +
                       "': " + ec.message() + ". Complete copy retained at '" +
                       Utils::path_to_utf8(destination) + "'."};
    }
    return StorageMutationResult{.success = true};
}
