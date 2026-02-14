#include "Updater.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "app_version.hpp"
#include <curl/curl.h>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#if __has_include(<jsoncpp/json/json.h>)
    #include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
    #include <json/json.h>
#else
    #error "jsoncpp headers not found. Install jsoncpp development files."
#endif
#include <optional>
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
#include <QUrl>

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
    update_spec_file_url([]
    {
        const char* url = std::getenv("UPDATE_SPEC_FILE_URL");
        if (!url) {
            throw std::runtime_error("Environment variable UPDATE_SPEC_FILE_URL is not set");
        }
        return std::string(url);
    }())
{}


void Updater::check_updates()
{
    std::string update_json = fetch_update_metadata();
    Json::CharReaderBuilder readerBuilder;
    Json::Value root;
    std::string errors;

    std::unique_ptr<Json::CharReader> reader(readerBuilder.newCharReader());
    if (!reader->parse(update_json.c_str(), update_json.c_str() + update_json.length(), &root, &errors)) {
        throw std::runtime_error("JSON Parse Error: " + errors);
    }

    if (!root.isMember("update")) {
        update_info.reset();
        return;
    }

    const Json::Value update = root["update"];
    UpdateInfo info;

    if (update.isMember("current_version") && update["current_version"].isString()) {
        info.current_version = update["current_version"].asString();
    }

    if (update.isMember("min_version") && update["min_version"].isString()) {
        info.min_version = update["min_version"].asString();
    }
    
    if (update.isMember("download_url") && update["download_url"].isString()) {
        info.download_url = update["download_url"].asString();
    }

    if (APP_VERSION >= string_to_Version(info.current_version)) {
        update_info.reset();
        return;
    }

    update_info = std::make_optional(info);  // Store the update info
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
    Version skipped_version = string_to_Version(settings.get_skipped_version());
    return APP_VERSION <= skipped_version;
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


void Updater::show_required_update_dialog(const UpdateInfo& info, QWidget* parent) const
{
    QMessageBox box(parent);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(QObject::tr("Required Update Available"));
    box.setText(QObject::tr("A required update is available. Please update to continue.\nIf you choose to quit, the application will close."));
    QPushButton* update_now = box.addButton(QObject::tr("Update Now"), QMessageBox::AcceptRole);
    QPushButton* quit_button = box.addButton(QObject::tr("Quit"), QMessageBox::RejectRole);
    box.setDefaultButton(update_now);
    box.exec();

    if (box.clickedButton() == update_now) {
        open_download_url(info.download_url);
        QApplication::quit();
    } else if (box.clickedButton() == quit_button) {
        QApplication::quit();
    }
}


void Updater::show_optional_update_dialog(const UpdateInfo& info, QWidget* parent) const
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
        open_download_url(info.download_url);
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


size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    const auto total = size * nmemb;
    auto* buffer = static_cast<std::string*>(userp);
    buffer->append(static_cast<const char*>(contents), total);
    return total;
}


std::string Updater::fetch_update_metadata() const {
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

    curl_easy_setopt(curl, CURLOPT_URL, update_spec_file_url.c_str());
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
    std::vector<int> digits;
    std::istringstream stream(version_str);
    std::string segment;

    while (std::getline(stream, segment, '.')) {
        digits.push_back(std::stoi(segment));
    }

    return Version{digits};
}


Updater::~Updater() = default;
