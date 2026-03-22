#pragma once

#include "Settings.hpp"
#include "UpdateFeed.hpp"

#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

struct UpdatePreparationResult
{
    enum class Status {
        Ready,
        Canceled,
        Failed
    };

    Status status{Status::Failed};
    std::filesystem::path installer_path;
    std::string message;

    static UpdatePreparationResult ready(std::filesystem::path path)
    {
        return UpdatePreparationResult{Status::Ready, std::move(path), {}};
    }

    static UpdatePreparationResult canceled(std::string message = {})
    {
        return UpdatePreparationResult{Status::Canceled, {}, std::move(message)};
    }

    static UpdatePreparationResult failed(std::string message)
    {
        return UpdatePreparationResult{Status::Failed, {}, std::move(message)};
    }
};

class UpdateInstaller
{
public:
    struct LaunchRequest
    {
        std::string program;
        std::vector<std::string> arguments;
    };

    class DownloadCanceledError : public std::runtime_error
    {
    public:
        DownloadCanceledError();
    };

    using ProgressCallback = std::function<void(double, const std::string&)>;
    using CancelCheck = std::function<bool()>;
    using DownloadFunction = std::function<void(const std::string&,
                                                const std::filesystem::path&,
                                                ProgressCallback,
                                                CancelCheck)>;
    using LaunchFunction = std::function<bool(const std::filesystem::path&)>;

    explicit UpdateInstaller(Settings& settings,
                             DownloadFunction download_fn = {},
                             LaunchFunction launch_fn = {});

    bool supports_auto_install(const UpdateInfo& info) const;
    UpdatePreparationResult prepare(const UpdateInfo& info,
                                    ProgressCallback progress_cb = {},
                                    CancelCheck cancel_check = {}) const;
    bool launch(const std::filesystem::path& installer_path) const;

private:
    Settings& settings_;
    DownloadFunction download_fn_;
    LaunchFunction launch_fn_;

    std::filesystem::path updates_dir() const;
    std::filesystem::path package_path_for(const UpdateInfo& info) const;
    std::filesystem::path extracted_installer_root_for(const std::filesystem::path& package_path) const;
    std::string compute_sha256(const std::filesystem::path& path) const;
    bool verify_file(const std::filesystem::path& path, const std::string& expected_sha256) const;
    static LaunchRequest build_launch_request(const std::filesystem::path& installer_path);

    static void default_download(const std::string& url,
                                 const std::filesystem::path& destination_path,
                                 ProgressCallback progress_cb,
                                 CancelCheck cancel_check);
    static bool default_launch(const std::filesystem::path& installer_path);

#ifdef AI_FILE_SORTER_TEST_BUILD
    friend class UpdateInstallerTestAccess;
#endif
};
