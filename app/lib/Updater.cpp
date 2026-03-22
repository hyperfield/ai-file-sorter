#include "Updater.hpp"
#include "Logger.hpp"
#include "UpdaterBuildConfig.hpp"
#include "UpdaterLaunchOptions.hpp"
#include "Utils.hpp"
#include "app_version.hpp"
#include <curl/curl.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <curl/easy.h>
#include <future>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>
#include <QApplication>
#include <QDesktopServices>
#include <QMessageBox>
#include <QMetaObject>
#include <QObject>
#include <QPushButton>
#include <QProgressDialog>
#include <QUrl>

#ifdef AI_FILE_SORTER_TEST_BUILD
    #include "UpdaterTestAccess.hpp"
#endif

namespace {
template <typename... Args>
void updater_log(spdlog::level::level_enum level, const char* fmt, Args&&... args) {
    auto message = fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...);
    if (auto logger = Logger::get_logger("core_logger")) {
        logger->log(level, "{}", message);
    } else {
        std::fprintf(stderr, "%s\n", message.c_str());
    }
}

std::string trim_copy(const std::string& value)
{
    auto trimmed = value;
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), not_space));
    trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), not_space).base(), trimmed.end());
    return trimmed;
}

std::optional<std::string> env_string(const char* key)
{
    const char* value = std::getenv(key);
    if (!value || value[0] == '\0') {
        return std::nullopt;
    }
    const std::string trimmed = trim_copy(value);
    if (trimmed.empty()) {
        return std::nullopt;
    }
    return trimmed;
}

bool env_flag_enabled(const char* key)
{
    const auto value = env_string(key);
    if (!value) {
        return false;
    }

    std::string lowered = *value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on";
}

std::string normalized_sha256_copy(std::string value)
{
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }), value.end());
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

void throw_for_http_status(long http_code)
{
    if (http_code == 401) {
        throw std::runtime_error("Authentication Error: Invalid or missing API key.");
    }
    if (http_code == 403) {
        throw std::runtime_error("Authorization Error: API key does not have sufficient permissions.");
    }
    if (http_code >= 500) {
        throw std::runtime_error("Server Error: The server returned an error. Status code: " + std::to_string(http_code));
    }
    if (http_code >= 400) {
        throw std::runtime_error("Client Error: The server returned an error. Status code: " + std::to_string(http_code));
    }
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

void open_download_url(const std::string& url)
{
    const QUrl qurl(QString::fromStdString(url));
    if (!QDesktopServices::openUrl(qurl)) {
        updater_log(spdlog::level::err, "Failed to open URL: {}", url);
    }
}
}


Updater::Updater(Settings& settings) 
    :
    settings(settings),
    installer(settings),
    update_spec_file_url_(env_string("UPDATE_SPEC_FILE_URL")),
    open_download_url_fn_([](const std::string& url) { open_download_url(url); }),
    quit_fn_([]() { QApplication::quit(); })
{}


void Updater::check_updates()
{
    if (auto live_test = resolve_live_test_update()) {
        update_info = std::move(live_test);
    } else {
        std::string update_json = fetch_update_metadata();
        update_info = UpdateFeed::parse_for_current_platform(update_json);
    }
    if (!update_info) {
        update_info.reset();
        return;
    }

    if (APP_VERSION >= string_to_Version(update_info->current_version)) {
        update_info.reset();
    }
}

std::optional<UpdateInfo> Updater::resolve_live_test_update() const
{
#if !defined(_WIN32)
    return std::nullopt;
#else
    if (!env_flag_enabled(UpdaterLaunchOptions::kLiveTestModeEnv)) {
        return std::nullopt;
    }

    const auto installer_url = env_string(UpdaterLaunchOptions::kLiveTestUrlEnv);
    if (!installer_url) {
        throw std::runtime_error(
            "Updater live test mode requires an installer URL via "
            "--updater-live-test-url or AI_FILE_SORTER_UPDATER_TEST_URL.");
    }

    const auto installer_sha256 = env_string(UpdaterLaunchOptions::kLiveTestSha256Env);
    if (!installer_sha256) {
        throw std::runtime_error(
            "Updater live test mode requires an archive SHA-256 via "
            "--updater-live-test-sha256 or AI_FILE_SORTER_UPDATER_TEST_SHA256.");
    }

    UpdateInfo info;
    info.current_version = env_string(UpdaterLaunchOptions::kLiveTestVersionEnv)
                               .value_or(APP_VERSION.to_string() + ".1");
    info.min_version = env_string(UpdaterLaunchOptions::kLiveTestMinVersionEnv)
                           .value_or("0.0.0");
    info.download_url = *installer_url;
    info.installer_url = *installer_url;
    info.installer_sha256 = normalized_sha256_copy(*installer_sha256);

    updater_log(spdlog::level::info,
                "Updater live test mode enabled for package '{}'",
                info.installer_url);
    return info;
#endif
}


bool Updater::is_update_available()
{
    check_updates();
    return update_info.has_value();
}


bool Updater::is_update_required()
{
    return string_to_Version(update_info.value_or(UpdateInfo()).min_version) > APP_VERSION;
}


void Updater::begin()
{
    if (!UpdaterBuildConfig::update_checks_enabled()) {
        updater_log(spdlog::level::info, "Updater checks disabled for this build.");
        return;
    }

    this->update_future = std::async(std::launch::async, [this]() { 
        try {
            if (is_update_available()) {
                QMetaObject::invokeMethod(QApplication::instance(), [this]() {
                    if (is_update_required()) {
                        display_update_dialog(true);
                    } else if (!is_update_skipped()) {
                        display_update_dialog(false);
                    }
                }, Qt::QueuedConnection);
            } else {
                QMetaObject::invokeMethod(QApplication::instance(), []() {
                    std::cout << "No updates available.\n";
                }, Qt::QueuedConnection);
            }
        } catch (const std::exception &e) {
            QMetaObject::invokeMethod(QApplication::instance(), [msg = std::string(e.what())]() {
                updater_log(spdlog::level::err, "Updater encountered an error: {}", msg);
            }, Qt::QueuedConnection);
        }
    });
}


bool Updater::is_update_skipped()
{
    if (!update_info) {
        return false;
    }
    Version skipped_version = string_to_Version(settings.get_skipped_version());
    return string_to_Version(update_info->current_version) <= skipped_version;
}


void Updater::display_update_dialog(bool is_required) {
    if (!update_info) {
        updater_log(spdlog::level::warn, "No update information available.");
        return;
    }

    QWidget* parent = QApplication::activeWindow();
    const auto& info = update_info.value();

    if (is_required) {
        show_required_update_dialog(info, parent);
        return;
    }

    show_optional_update_dialog(info, parent);
}


void Updater::show_required_update_dialog(const UpdateInfo& info, QWidget* parent)
{
    while (true) {
        QMessageBox box(parent);
        box.setIcon(QMessageBox::Warning);
        box.setWindowTitle(QObject::tr("Required Update Available"));
        box.setText(QObject::tr("A required update is available. Please update to continue.\nIf you choose to quit, the application will close."));
        QPushButton* update_now = box.addButton(QObject::tr("Update Now"), QMessageBox::AcceptRole);
        QPushButton* quit_button = box.addButton(QObject::tr("Quit"), QMessageBox::RejectRole);
        box.setDefaultButton(update_now);
        box.exec();

        if (box.clickedButton() == update_now) {
            if (trigger_update_action(info, parent, true)) {
                return;
            }
            continue;
        }

        if (box.clickedButton() == quit_button) {
            quit_fn_();
            return;
        }
    }
}


void Updater::show_optional_update_dialog(const UpdateInfo& info, QWidget* parent)
{
    QMessageBox box(parent);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(QObject::tr("Optional Update Available"));
    box.setText(QObject::tr("An optional update is available. Would you like to update now?"));
    QPushButton* update_now = box.addButton(QObject::tr("Update Now"), QMessageBox::AcceptRole);
    QPushButton* skip_button = box.addButton(QObject::tr("Skip This Version"), QMessageBox::RejectRole);
    QPushButton* cancel_button = box.addButton(QObject::tr("Cancel"), QMessageBox::DestructiveRole);
    box.setDefaultButton(update_now);
    box.exec();

    if (box.clickedButton() == update_now) {
        trigger_update_action(info, parent, false);
    } else if (box.clickedButton() == skip_button) {
        settings.set_skipped_version(info.current_version);
        if (!settings.save()) {
            updater_log(spdlog::level::err, "Failed to save skipped version to settings.");
        } else {
            std::cout << "User chose to skip version " << info.current_version << "." << std::endl;
        }
    } else if (box.clickedButton() == cancel_button) {
        // No action needed; user dismissed the dialog.
    }
}

UpdatePreparationResult Updater::prepare_installer_update(const UpdateInfo& info, QWidget* parent)
{
    QProgressDialog progress(parent);
    progress.setWindowTitle(QObject::tr("Downloading Update"));
    progress.setLabelText(QObject::tr("Downloading the update installer..."));
    progress.setCancelButtonText(QObject::tr("Cancel"));
    progress.setRange(0, 100);
    progress.setValue(0);
    progress.setMinimumDuration(0);
    progress.setWindowModality(Qt::ApplicationModal);
    progress.show();
    QApplication::processEvents();

    auto result = installer.prepare(
        info,
        [&](double value, const std::string& status) {
            const double clamped = std::clamp(value, 0.0, 1.0);
            progress.setValue(static_cast<int>(clamped * 100.0));
            if (!status.empty()) {
                progress.setLabelText(QString::fromStdString(status));
            }
            QApplication::processEvents();
        },
        [&]() {
            QApplication::processEvents();
            return progress.wasCanceled();
        });

    progress.setValue(result.status == UpdatePreparationResult::Status::Ready ? 100 : progress.value());
    return result;
}

bool Updater::trigger_update_action(const UpdateInfo& info, QWidget* parent, bool quit_after_open)
{
#if defined(_WIN32)
    if (UpdaterBuildConfig::auto_install_enabled() && installer.supports_auto_install(info)) {
        const auto prepared = prepare_installer_update(info, parent);
        if (prepared.status == UpdatePreparationResult::Status::Canceled) {
            return false;
        }
        if (prepared.status == UpdatePreparationResult::Status::Failed) {
            return handle_update_error(info,
                                       QObject::tr("Failed to prepare the update installer.\n%1")
                                           .arg(QString::fromStdString(prepared.message)),
                                       parent,
                                       quit_after_open);
        }

        QMessageBox confirm(parent);
        confirm.setIcon(QMessageBox::Question);
        confirm.setWindowTitle(QObject::tr("Installer Ready"));
        confirm.setText(QObject::tr("Quit the app and launch the installer to update"));
        QPushButton* launch_button = confirm.addButton(QObject::tr("Quit and Launch Installer"),
                                                       QMessageBox::AcceptRole);
        QPushButton* cancel_button = confirm.addButton(QObject::tr("Cancel"),
                                                       QMessageBox::RejectRole);
        confirm.setDefaultButton(launch_button);
        confirm.exec();

        if (confirm.clickedButton() != launch_button) {
            return false;
        }

        if (!installer.launch(prepared.installer_path)) {
            return handle_update_error(info,
                                       QObject::tr("The installer could not be launched."),
                                       parent,
                                       quit_after_open);
        }

        quit_fn_();
        return true;
    }
#endif

    if (info.download_url.empty()) {
        return handle_update_error(info,
                                   QObject::tr("No download target is available for this update."),
                                   parent,
                                   quit_after_open);
    }

    open_download_url_fn_(info.download_url);
    if (quit_after_open) {
        quit_fn_();
    }
    return true;
}

bool Updater::handle_update_error(const UpdateInfo& info,
                                  const QString& message,
                                  QWidget* parent,
                                  bool quit_after_open)
{
    QMessageBox box(parent);
    box.setIcon(QMessageBox::Critical);
    box.setWindowTitle(QObject::tr("Update Failed"));
    box.setText(message);
    QPushButton* ok_button = box.addButton(QMessageBox::Ok);
    QPushButton* manual_button = nullptr;
    if (!info.download_url.empty()) {
        manual_button = box.addButton(QObject::tr("Update manually"), QMessageBox::ActionRole);
    }
    box.setDefaultButton(ok_button);
    box.exec();

    if (box.clickedButton() != manual_button) {
        return false;
    }

    open_download_url_fn_(info.download_url);
    if (quit_after_open) {
        quit_fn_();
    }
    return true;
}


size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    const auto total = size * nmemb;
    auto* buffer = static_cast<std::string*>(userp);
    buffer->append(static_cast<const char*>(contents), total);
    return total;
}


std::string Updater::fetch_update_metadata() const {
    if (!update_spec_file_url_) {
        throw std::runtime_error("Environment variable UPDATE_SPEC_FILE_URL is not set");
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Initialization Error: Failed to initialize cURL.");
    }

    CURLcode res;
    std::string response_string;

    try {
        configure_tls(curl);
    } catch (const std::exception& ex) {
        curl_easy_cleanup(curl);
        throw std::runtime_error(std::string("Failed to stage CA bundle: ") + ex.what());
    }

    curl_easy_setopt(curl, CURLOPT_URL, update_spec_file_url_->c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Perform the request
    res = curl_easy_perform(curl);

    // Handle errors
    if (res != CURLE_OK) {
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        throw std::runtime_error("Network Error: " + std::string(curl_easy_strerror(res)));
    }

    // Check HTTP response code
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    throw_for_http_status(http_code);

    return response_string;
}


Version Updater::string_to_Version(const std::string& version_str) {
    if (version_str.empty()) {
        return Version{0, 0, 0};
    }

    std::vector<int> digits;
    std::istringstream stream(version_str);
    std::string segment;

    while (std::getline(stream, segment, '.')) {
        if (segment.empty()) {
            throw std::runtime_error("Invalid version string: " + version_str);
        }
        digits.push_back(std::stoi(segment));
    }

    if (digits.empty()) {
        return Version{0, 0, 0};
    }

    return Version{digits};
}


Updater::~Updater() = default;

#ifdef AI_FILE_SORTER_TEST_BUILD
bool UpdaterTestAccess::is_update_available(Updater& updater)
{
    return updater.is_update_available();
}

std::optional<UpdateInfo> UpdaterTestAccess::current_update_info(const Updater& updater)
{
    return updater.update_info;
}

bool UpdaterTestAccess::has_update_task(const Updater& updater)
{
    return updater.update_future.valid();
}

void UpdaterTestAccess::wait_for_update_task(Updater& updater)
{
    if (updater.update_future.valid()) {
        updater.update_future.wait();
    }
}

void UpdaterTestAccess::set_open_download_url_handler(Updater& updater,
                                                      std::function<void(const std::string&)> handler)
{
    updater.open_download_url_fn_ = std::move(handler);
}

void UpdaterTestAccess::set_quit_handler(Updater& updater,
                                         std::function<void()> handler)
{
    updater.quit_fn_ = std::move(handler);
}

bool UpdaterTestAccess::trigger_update_action(Updater& updater,
                                              const UpdateInfo& info,
                                              QWidget* parent,
                                              bool quit_after_open)
{
    return updater.trigger_update_action(info, parent, quit_after_open);
}

bool UpdaterTestAccess::handle_update_error(Updater& updater,
                                            const UpdateInfo& info,
                                            const QString& message,
                                            QWidget* parent,
                                            bool quit_after_open)
{
    return updater.handle_update_error(info, message, parent, quit_after_open);
}
#endif
