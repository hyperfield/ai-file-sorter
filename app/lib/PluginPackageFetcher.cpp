#include "PluginPackageFetcher.hpp"

#include <curl/curl.h>
#include <curl/easy.h>
#include <spdlog/fmt/fmt.h>

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <system_error>
#include <utility>

#include "Logger.hpp"
#include "Utils.hpp"

namespace {

template <typename... Args>
void log_plugin_fetch(spdlog::level::level_enum level, const char* fmt, Args&&... args) {
    auto message = fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...);
    if (auto logger = Logger::get_logger("core_logger")) {
        logger->log(level, "{}", message);
    } else {
        std::fprintf(stderr, "%s\n", message.c_str());
    }
}

void configure_tls(CURL* curl) {
#if defined(_WIN32)
    const auto cert_path = Utils::ensure_ca_bundle();
    curl_easy_setopt(curl, CURLOPT_CAINFO, cert_path.string().c_str());
#else
    (void) curl;
#endif
}

FILE* open_binary_file_for_write(const std::filesystem::path& path) {
#ifdef _WIN32
    return _wfopen(path.c_str(), L"wb");
#else
    return std::fopen(path.c_str(), "wb");
#endif
}

size_t write_data(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    return std::fwrite(ptr, size, nmemb, stream);
}

struct DownloadContext {
    PluginPackageFetcher::ProgressCallback progress_cb;
    PluginPackageFetcher::CancelCheck cancel_check;
};

int progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t) {
    auto* context = static_cast<DownloadContext*>(clientp);
    if (context->cancel_check && context->cancel_check()) {
        return 1;
    }
    if (context->progress_cb) {
        const double progress = dltotal > 0 ? static_cast<double>(dlnow) / static_cast<double>(dltotal) : 0.0;
        context->progress_cb(progress, "Downloading plugin package");
    }
    return 0;
}

bool replace_file(const std::filesystem::path& source, const std::filesystem::path& target, std::string* error) {
    std::error_code ec;
    std::filesystem::remove(target, ec);
    ec.clear();
    std::filesystem::rename(source, target, ec);
    if (!ec) {
        return true;
    }
    if (error) {
        *error = ec.message();
    }
    return false;
}

}  // namespace

PluginPackageFetcher::PluginPackageFetcher(std::filesystem::path cache_directory, DownloadFunction download_fn)
    : cache_directory_(std::move(cache_directory)),
      download_fn_(download_fn ? std::move(download_fn) : default_download) {}

bool PluginPackageFetcher::fetch_to_cache(const std::string& url, const std::filesystem::path& cache_path,
                                          std::string* error) const {
    if (url.empty()) {
        if (error) {
            *error = "No plugin URL is configured.";
        }
        return false;
    }
    if (cache_path.empty()) {
        if (error) {
            *error = "No plugin cache path was provided.";
        }
        return false;
    }

    const auto partial_path = std::filesystem::path(cache_path.string() + ".part");

    std::error_code ec;
    std::filesystem::create_directories(cache_path.parent_path(), ec);
    if (ec) {
        if (error) {
            *error = "Failed to create plugin cache directory: " + ec.message();
        }
        return false;
    }

    std::filesystem::remove(partial_path, ec);
    ec.clear();

    try {
        download_fn_(url, partial_path, {}, {});
    } catch (const std::exception& ex) {
        std::filesystem::remove(partial_path, ec);
        if (error) {
            *error = ex.what();
        }
        return false;
    }

    std::string replace_error;
    if (!replace_file(partial_path, cache_path, &replace_error)) {
        std::filesystem::remove(partial_path, ec);
        if (error) {
            *error = "Failed to finalize downloaded plugin file: " + replace_error;
        }
        return false;
    }

    return true;
}

std::optional<std::string> PluginPackageFetcher::fetch_text(const std::string& url,
                                                            const std::filesystem::path& cache_path,
                                                            std::string* error) const {
    if (!fetch_to_cache(url, cache_path, error)) {
        return std::nullopt;
    }

    QFile file(QString::fromStdString(Utils::path_to_utf8(cache_path)));
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = "Failed to open cached plugin file.";
        }
        return std::nullopt;
    }
    const QByteArray bytes = file.readAll();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

bool PluginPackageFetcher::fetch_package_archive(const PluginPackageSource& source, std::filesystem::path* archive_path,
                                                 std::string* error) const {
    if (!archive_path) {
        if (error) {
            *error = "No archive output path was provided.";
        }
        return false;
    }
    if (source.package_download_url.empty()) {
        if (error) {
            *error = "No plugin package URL is configured for this plugin.";
        }
        return false;
    }
    if (source.package_sha256.empty()) {
        if (error) {
            *error = "No plugin package SHA-256 is configured for this plugin.";
        }
        return false;
    }

    const auto package_path = package_cache_path(cache_directory_, source);
    const auto partial_path = std::filesystem::path(package_path.string() + ".part");

    std::error_code ec;
    std::filesystem::create_directories(package_path.parent_path(), ec);
    if (ec) {
        if (error) {
            *error = "Failed to create plugin cache directory: " + ec.message();
        }
        return false;
    }

    if (verify_file(package_path, source.package_sha256)) {
        *archive_path = package_path;
        return true;
    }

    std::filesystem::remove(partial_path, ec);
    ec.clear();

    try {
        download_fn_(source.package_download_url, partial_path, {}, {});
    } catch (const std::exception& ex) {
        std::filesystem::remove(partial_path, ec);
        if (error) {
            *error = ex.what();
        }
        return false;
    }

    if (!verify_file(partial_path, source.package_sha256)) {
        std::filesystem::remove(partial_path, ec);
        if (error) {
            *error = "The downloaded plugin archive failed SHA-256 verification.";
        }
        return false;
    }

    std::string replace_error;
    if (!replace_file(partial_path, package_path, &replace_error)) {
        std::filesystem::remove(partial_path, ec);
        if (error) {
            *error = "Failed to finalize downloaded plugin archive: " + replace_error;
        }
        return false;
    }

    *archive_path = package_path;
    return true;
}

std::filesystem::path PluginPackageFetcher::package_cache_path(const std::filesystem::path& cache_directory,
                                                               const PluginPackageSource& source) {
    const std::string fallback = source.plugin_id.empty() ? "plugin.aifsplugin" : source.plugin_id + ".aifsplugin";
    return cache_directory / source.plugin_id / source.version /
           safe_file_name_from_url(source.package_download_url, fallback);
}

std::string PluginPackageFetcher::normalize_sha256(std::string value) {
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch) != 0; }),
                value.end());
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::string PluginPackageFetcher::safe_file_name_from_url(const std::string& url, std::string fallback_stem) {
    const QUrl parsed = QUrl::fromUserInput(QString::fromStdString(url));
    QString file_name = QFileInfo(parsed.path()).fileName();
    if (file_name.isEmpty()) {
        file_name = QString::fromStdString(std::move(fallback_stem));
    }
    return file_name.toStdString();
}

std::string PluginPackageFetcher::compute_sha256(const std::filesystem::path& path) {
    QFile file(QString::fromStdString(Utils::path_to_utf8(path)));
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("Failed to open plugin package for SHA-256 verification.");
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(1 << 20);
        if (chunk.isEmpty() && file.error() != QFile::NoError) {
            throw std::runtime_error("Failed while reading plugin package for SHA-256 verification.");
        }
        hash.addData(chunk);
    }

    return hash.result().toHex().toStdString();
}

bool PluginPackageFetcher::verify_file(const std::filesystem::path& path, const std::string& expected_sha256) {
    const auto normalized_sha = normalize_sha256(expected_sha256);
    if (normalized_sha.empty()) {
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        return false;
    }

    try {
        return compute_sha256(path) == normalized_sha;
    } catch (const std::exception& ex) {
        log_plugin_fetch(spdlog::level::warn, "Failed to verify plugin package '{}': {}", path.string(), ex.what());
        return false;
    }
}

void PluginPackageFetcher::default_download(const std::string& url, const std::filesystem::path& destination_path,
                                            ProgressCallback progress_cb, CancelCheck cancel_check) {
    std::error_code ec;
    std::filesystem::create_directories(destination_path.parent_path(), ec);
    if (ec) {
        throw std::runtime_error("Failed to create plugin download directory: " + ec.message());
    }

    FILE* file = open_binary_file_for_write(destination_path);
    if (!file) {
        throw std::runtime_error("Failed to open plugin download destination.");
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::fclose(file);
        throw std::runtime_error("Failed to initialize libcurl for plugin download.");
    }

    DownloadContext context{std::move(progress_cb), std::move(cancel_check)};

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &context);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "AI-File-Sorter-PluginInstaller/1.0");
    configure_tls(curl);

    const CURLcode result = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);
    std::fclose(file);

    if (result != CURLE_OK) {
        throw std::runtime_error("Plugin download failed for " + url + ": " + std::string(curl_easy_strerror(result)));
    }
    if (http_code < 200 || http_code >= 300) {
        throw std::runtime_error("Plugin download returned HTTP " + std::to_string(http_code) + " for " + url + ".");
    }
}
