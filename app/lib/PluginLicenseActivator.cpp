#include "PluginLicenseActivator.hpp"

#include <curl/curl.h>

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QString>
#include <cstddef>
#include <cstdlib>
#include <stdexcept>
#include <utility>

#include "PluginEntitlementService.hpp"
#include "Utils.hpp"

namespace {

struct JsonHttpResponse {
    long status_code{0};
    std::string body;
};

QByteArray base64url_decode_value(const QJsonValue& value) {
    if (!value.isString()) {
        return {};
    }
    QByteArray encoded = value.toString().trimmed().toUtf8();
    const qsizetype remainder = encoded.size() % 4;
    if (remainder > 0) {
        encoded.append(QByteArray(4 - remainder, '='));
    }
    return QByteArray::fromBase64(encoded, QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
}

std::size_t write_curl_response(char* contents, std::size_t size, std::size_t nmemb, void* user_data) {
    const std::size_t total_size = size * nmemb;
    auto* response = static_cast<std::string*>(user_data);
    response->append(contents, total_size);
    return total_size;
}

JsonHttpResponse post_json(const std::string& url, const QJsonObject& payload) {
    struct CurlHandle {
        CURL* handle{curl_easy_init()};
        ~CurlHandle() {
            if (handle) {
                curl_easy_cleanup(handle);
            }
        }
    } curl;
    if (!curl.handle) {
        throw std::runtime_error("Failed to initialize cURL.");
    }

    struct HeaderList {
        curl_slist* list{nullptr};
        ~HeaderList() { curl_slist_free_all(list); }
        void append(const char* header) {
            curl_slist* next = curl_slist_append(list, header);
            if (!next) {
                throw std::runtime_error("Failed to allocate HTTP request headers.");
            }
            list = next;
        }
    } headers;
    headers.append("Accept: application/json");
    headers.append("Content-Type: application/json");

    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    std::string response_body;
    char error_buffer[CURL_ERROR_SIZE] = {};
    std::string ca_bundle_path;

    curl_easy_setopt(curl.handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.handle, CURLOPT_POST, 1L);
    curl_easy_setopt(curl.handle, CURLOPT_POSTFIELDS, body.constData());
    curl_easy_setopt(curl.handle, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.size()));
    curl_easy_setopt(curl.handle, CURLOPT_HTTPHEADER, headers.list);
    curl_easy_setopt(curl.handle, CURLOPT_WRITEFUNCTION, write_curl_response);
    curl_easy_setopt(curl.handle, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl.handle, CURLOPT_ERRORBUFFER, error_buffer);
    curl_easy_setopt(curl.handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.handle, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl.handle, CURLOPT_USERAGENT, "AI-File-Sorter-PluginActivator/1.0");

    if (url.rfind("https://", 0) == 0) {
        ca_bundle_path = Utils::path_to_utf8(Utils::ensure_ca_bundle());
        curl_easy_setopt(curl.handle, CURLOPT_CAINFO, ca_bundle_path.c_str());
    }

    const CURLcode result = curl_easy_perform(curl.handle);
    if (result != CURLE_OK) {
        const char* message = error_buffer[0] != '\0' ? error_buffer : curl_easy_strerror(result);
        throw std::runtime_error("Plugin activation request failed: " + std::string(message));
    }

    long status_code = 0;
    curl_easy_getinfo(curl.handle, CURLINFO_RESPONSE_CODE, &status_code);
    return JsonHttpResponse{status_code, std::move(response_body)};
}

std::string activation_error_message(const QJsonObject& response_object, const std::string& response_body) {
    std::string message = response_object.value("message").toString().trimmed().toStdString();
    if (!message.empty()) {
        return message;
    }
    message = response_object.value("error").toString().trimmed().toStdString();
    if (!message.empty()) {
        return message;
    }
    return response_body.substr(0, 500);
}

PluginLicenseActivationResult failed_activation(std::string message, long status_code = 0, QJsonObject response = {}) {
    PluginLicenseActivationResult result;
    result.http_status = status_code;
    result.message = std::move(message);
    result.response = std::move(response);
    return result;
}

}  // namespace

std::string PluginLicenseActivator::default_activation_url() {
    const char* env_value = std::getenv("AI_FILE_SORTER_PLUGIN_ACTIVATION_URL");
    if (env_value && env_value[0] != '\0') {
        return std::string(env_value);
    }
    return "https://filesorter.app/api/plugins/activate/";
}

std::string PluginLicenseActivator::default_platform() {
#ifdef _WIN32
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#else
    return "linux";
#endif
}

PluginLicenseActivationResult PluginLicenseActivator::activate(const std::filesystem::path& config_dir,
                                                               const PluginLicenseActivationRequest& request) {
    const std::string license_key = request.license_key;
    const std::string product_id = request.product_id;
    if (license_key.empty() || product_id.empty()) {
        return failed_activation("Plugin activation requires a license key and product id.");
    }

    PluginEntitlementService entitlement_service(config_dir);
    QJsonObject payload;
    payload.insert("license_key", QString::fromStdString(license_key));
    payload.insert("product_id", QString::fromStdString(product_id));
    payload.insert("device_id", QString::fromStdString(entitlement_service.device_id()));
    payload.insert("app_version", QString::fromStdString(request.app_version));
    payload.insert("platform",
                   QString::fromStdString(request.platform.empty() ? default_platform() : request.platform));

    JsonHttpResponse response;
    try {
        response =
            post_json(request.activation_url.empty() ? default_activation_url() : request.activation_url, payload);
    } catch (const std::exception& e) {
        return failed_activation(e.what());
    }

    QJsonParseError parse_error;
    const QJsonDocument response_doc = QJsonDocument::fromJson(QByteArray::fromStdString(response.body), &parse_error);
    const QJsonObject response_object = response_doc.isObject() ? response_doc.object() : QJsonObject();
    if (parse_error.error != QJsonParseError::NoError || !response_doc.isObject()) {
        return failed_activation("Plugin activation response was not valid JSON.", response.status_code);
    }

    if (response.status_code < 200 || response.status_code >= 300) {
        return failed_activation("Plugin activation failed (HTTP " + std::to_string(response.status_code) +
                                     "): " + activation_error_message(response_object, response.body),
                                 response.status_code, response_object);
    }

    const QByteArray receipt_payload = base64url_decode_value(response_object.value("payload"));
    const QByteArray signature = base64url_decode_value(response_object.value("signature"));
    const std::string key_id = response_object.value("key_id").toString().trimmed().toStdString();
    if (receipt_payload.isEmpty() || signature.size() != 64 || key_id.empty()) {
        return failed_activation("Plugin activation response did not include a complete signed receipt.",
                                 response.status_code, response_object);
    }

    std::string store_error;
    if (!entitlement_service.store_receipt(receipt_payload, signature, key_id, &store_error)) {
        return failed_activation("Plugin entitlement receipt was rejected: " + store_error, response.status_code,
                                 response_object);
    }

    PluginLicenseActivationResult result;
    result.activated = true;
    result.http_status = response.status_code;
    result.message = "Plugin license activated.";
    result.response = response_object;
    return result;
}
