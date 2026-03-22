#include "UpdateInstaller.hpp"

#include "Logger.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

#include <curl/curl.h>
#include <curl/easy.h>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QUrl>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#ifdef AI_FILE_SORTER_TEST_BUILD
    #include "UpdateInstallerTestAccess.hpp"
#endif

#ifdef _WIN32
    #include <windows.h>
#endif

namespace {

template <typename... Args>
void update_installer_log(spdlog::level::level_enum level, const char* fmt, Args&&... args)
{
    auto message = fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...);
    if (auto logger = Logger::get_logger("core_logger")) {
        logger->log(level, "{}", message);
    } else {
        std::fprintf(stderr, "%s\n", message.c_str());
    }
}

std::string ascii_lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

void configure_tls(CURL* curl)
{
#if defined(_WIN32)
    const auto cert_path = Utils::ensure_ca_bundle();
    curl_easy_setopt(curl, CURLOPT_CAINFO, cert_path.string().c_str());
#else
    (void)curl;
#endif
}

std::string normalize_sha256(std::string value)
{
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }), value.end());
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string safe_version_fragment(std::string version)
{
    std::transform(version.begin(), version.end(), version.begin(), [](unsigned char ch) {
        if (std::isalnum(ch) || ch == '.' || ch == '_' || ch == '-') {
            return static_cast<char>(ch);
        }
        return '_';
    });
    return version;
}

std::string installer_file_name_from_url(const std::string& url)
{
    const QUrl parsed = QUrl::fromUserInput(QString::fromStdString(url));
    QString file_name = QFileInfo(parsed.path()).fileName();
    if (file_name.isEmpty()) {
        file_name = QStringLiteral("aifs-update-installer");
    }
    return file_name.toStdString();
}

bool replace_file(const std::filesystem::path& source,
                  const std::filesystem::path& target,
                  std::string& error)
{
#ifdef _WIN32
    if (MoveFileExW(source.c_str(),
                    target.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return true;
    }
    error = std::system_category().message(static_cast<int>(GetLastError()));
    return false;
#else
    std::error_code ec;
    std::filesystem::rename(source, target, ec);
    if (!ec) {
        return true;
    }
    error = ec.message();
    return false;
#endif
}

FILE* open_binary_file_for_write(const std::filesystem::path& path)
{
#ifdef _WIN32
    return _wfopen(path.c_str(), L"wb");
#else
    return std::fopen(path.c_str(), "wb");
#endif
}

size_t write_data(void* ptr, size_t size, size_t nmemb, FILE* stream)
{
    return std::fwrite(ptr, size, nmemb, stream);
}

struct DownloadContext {
    UpdateInstaller::ProgressCallback progress_cb;
    UpdateInstaller::CancelCheck cancel_check;
};

int progress_callback(void* clientp,
                      curl_off_t dltotal,
                      curl_off_t dlnow,
                      curl_off_t,
                      curl_off_t)
{
    auto* context = static_cast<DownloadContext*>(clientp);
    if (context->cancel_check && context->cancel_check()) {
        return 1;
    }
    if (context->progress_cb) {
        const double progress =
            dltotal > 0 ? static_cast<double>(dlnow) / static_cast<double>(dltotal) : 0.0;
        const std::string message =
            dltotal > 0
                ? fmt::format("Downloaded {} / {}",
                              Utils::format_size(dlnow),
                              Utils::format_size(dltotal))
                : fmt::format("Downloaded {}", Utils::format_size(dlnow));
        context->progress_cb(progress, message);
    }
    return 0;
}

} // namespace

UpdateInstaller::UpdateInstaller(Settings& settings,
                                 DownloadFunction download_fn,
                                 LaunchFunction launch_fn)
    : settings_(settings),
      download_fn_(download_fn ? std::move(download_fn) : UpdateInstaller::default_download),
      launch_fn_(launch_fn ? std::move(launch_fn) : UpdateInstaller::default_launch)
{
}

UpdateInstaller::DownloadCanceledError::DownloadCanceledError()
    : std::runtime_error("Download cancelled")
{
}

bool UpdateInstaller::supports_auto_install(const UpdateInfo& info) const
{
#if defined(_WIN32)
    return !info.installer_url.empty() && !info.installer_sha256.empty();
#else
    (void)info;
    return false;
#endif
}

UpdatePreparationResult UpdateInstaller::prepare(const UpdateInfo& info,
                                                 ProgressCallback progress_cb,
                                                 CancelCheck cancel_check) const
{
    if (info.installer_url.empty()) {
        return UpdatePreparationResult::failed("No installer URL is available for this update.");
    }
    if (info.installer_sha256.empty()) {
        return UpdatePreparationResult::failed("No installer SHA-256 is available for this update.");
    }

    const auto final_path = installer_path_for(info);
    const auto partial_path = std::filesystem::path(final_path.string() + ".part");

    std::error_code ec;
    std::filesystem::create_directories(final_path.parent_path(), ec);
    if (ec) {
        return UpdatePreparationResult::failed("Failed to create updater cache directory: " + ec.message());
    }

    if (progress_cb) {
        progress_cb(0.0, "Checking cached installer");
    }
    if (verify_file(final_path, info.installer_sha256)) {
        return UpdatePreparationResult::ready(final_path);
    }

    std::filesystem::remove(final_path, ec);
    ec.clear();
    std::filesystem::remove(partial_path, ec);

    try {
        download_fn_(info.installer_url, partial_path, progress_cb, cancel_check);
        if (progress_cb) {
            progress_cb(1.0, "Verifying installer");
        }
        if (!verify_file(partial_path, info.installer_sha256)) {
            std::filesystem::remove(partial_path, ec);
            return UpdatePreparationResult::failed("The downloaded installer failed SHA-256 verification.");
        }

        std::string replace_error;
        if (!replace_file(partial_path, final_path, replace_error)) {
            std::filesystem::remove(partial_path, ec);
            return UpdatePreparationResult::failed("Failed to finalize installer download: " + replace_error);
        }
        return UpdatePreparationResult::ready(final_path);
    } catch (const DownloadCanceledError&) {
        std::filesystem::remove(partial_path, ec);
        return UpdatePreparationResult::canceled("Download cancelled");
    } catch (const std::exception& ex) {
        std::filesystem::remove(partial_path, ec);
        return UpdatePreparationResult::failed(ex.what());
    }
}

bool UpdateInstaller::launch(const std::filesystem::path& installer_path) const
{
    return launch_fn_(installer_path);
}

std::filesystem::path UpdateInstaller::updates_dir() const
{
    return Utils::utf8_to_path(settings_.get_config_dir()) / "updates";
}

std::filesystem::path UpdateInstaller::installer_path_for(const UpdateInfo& info) const
{
    const std::string file_name = installer_file_name_from_url(info.installer_url);
    const std::string version_prefix = "v" + safe_version_fragment(info.current_version) + "-";
    return updates_dir() / (version_prefix + file_name);
}

std::string UpdateInstaller::compute_sha256(const std::filesystem::path& path) const
{
    QFile file(QString::fromStdString(Utils::path_to_utf8(path)));
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("Failed to open installer for SHA-256 verification.");
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(1 << 20);
        if (chunk.isEmpty() && file.error() != QFile::NoError) {
            throw std::runtime_error("Failed while reading installer for SHA-256 verification.");
        }
        hash.addData(chunk);
    }

    return hash.result().toHex().toStdString();
}

bool UpdateInstaller::verify_file(const std::filesystem::path& path, const std::string& expected_sha256) const
{
    if (expected_sha256.empty()) {
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        return false;
    }

    try {
        return compute_sha256(path) == normalize_sha256(expected_sha256);
    } catch (const std::exception& ex) {
        update_installer_log(spdlog::level::warn,
                             "Failed to verify installer '{}': {}",
                             path.string(),
                             ex.what());
        return false;
    }
}

UpdateInstaller::LaunchRequest UpdateInstaller::build_launch_request(const std::filesystem::path& installer_path)
{
    const std::string installer = Utils::path_to_utf8(installer_path);
    if (ascii_lower_copy(installer_path.extension().string()) == ".msi") {
        return LaunchRequest{
            "msiexec.exe",
            {"/i", installer}
        };
    }
    return LaunchRequest{installer, {}};
}

void UpdateInstaller::default_download(const std::string& url,
                                       const std::filesystem::path& destination_path,
                                       ProgressCallback progress_cb,
                                       CancelCheck cancel_check)
{
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize cURL for installer download.");
    }

    FILE* file = open_binary_file_for_write(destination_path);
    if (!file) {
        curl_easy_cleanup(curl);
        throw std::runtime_error("Failed to open the installer destination for writing.");
    }

    DownloadContext context{
        std::move(progress_cb),
        std::move(cancel_check)
    };

    try {
        configure_tls(curl);
    } catch (const std::exception& ex) {
        std::fclose(file);
        curl_easy_cleanup(curl);
        throw std::runtime_error(std::string("Failed to stage CA bundle: ") + ex.what());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, &progress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &context);

    const CURLcode res = curl_easy_perform(curl);
    std::fclose(file);
    curl_easy_cleanup(curl);

    if (res == CURLE_ABORTED_BY_CALLBACK) {
        throw DownloadCanceledError();
    }
    if (res != CURLE_OK) {
        throw std::runtime_error(std::string("Installer download failed: ") + curl_easy_strerror(res));
    }
}

bool UpdateInstaller::default_launch(const std::filesystem::path& installer_path)
{
#if defined(_WIN32)
    const auto request = build_launch_request(installer_path);
    QStringList arguments;
    arguments.reserve(static_cast<qsizetype>(request.arguments.size()));
    for (const auto& argument : request.arguments) {
        arguments.push_back(QString::fromStdString(argument));
    }
    return QProcess::startDetached(QString::fromStdString(request.program), arguments);
#else
    (void)installer_path;
    return false;
#endif
}

#ifdef AI_FILE_SORTER_TEST_BUILD
UpdateInstaller::LaunchRequest UpdateInstallerTestAccess::build_launch_request(const std::filesystem::path& installer_path)
{
    return UpdateInstaller::build_launch_request(installer_path);
}
#endif
