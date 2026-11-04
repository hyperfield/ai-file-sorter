#include "ChatCompletionTransport.hpp"

#include "Logger.hpp"
#include "RemoteApiError.hpp"
#include "Utils.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <curl/curl.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <utility>

namespace {
constexpr long kDefaultCustomTimeoutSeconds = 60;
constexpr long kOpenAiTimeoutSeconds = 5;
constexpr long kConnectTimeoutSeconds = 10;
constexpr size_t kMaxResponseBytes = 8 * 1024 * 1024;
constexpr auto kChatSuffix = "/chat/completions";

struct CurlRequest {
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> handle{curl_easy_init(), curl_easy_cleanup};
    curl_slist* headers{nullptr};
    ~CurlRequest() { curl_slist_free_all(headers); }
};

struct Response {
    std::string body;
    std::string retry_after;
};

std::string trimmed(const std::string& value)
{
    return QString::fromStdString(value).trimmed().toStdString();
}

std::string completion_url(const std::string& base_url)
{
    auto url = trimmed(base_url);
    if (url.empty()) {
        return "https://api.openai.com/v1/chat/completions";
    }
    while (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
    const std::string suffix(kChatSuffix);
    if (url.size() < suffix.size() || url.compare(url.size() - suffix.size(), suffix.size(), suffix) != 0) {
        url += suffix;
    }
    return url;
}

long timeout_seconds(const std::string& base_url)
{
    if (trimmed(base_url).empty()) {
        return kOpenAiTimeoutSeconds;
    }
    if (const char* value = std::getenv("AI_FILE_SORTER_CUSTOM_LLM_TIMEOUT")) {
        char* end = nullptr;
        const long seconds = std::strtol(value, &end, 10);
        if (end != value && *end == '\0' && seconds > 0) {
            return seconds;
        }
    }
    return kDefaultCustomTimeoutSeconds;
}

size_t receive_body(char* data, size_t size, size_t count, Response* response)
{
    const size_t bytes = size * count;
    if (bytes > kMaxResponseBytes - response->body.size()) {
        return 0;
    }
    response->body.append(data, bytes);
    return bytes;
}

size_t receive_header(char* data, size_t size, size_t count, Response* response)
{
    const size_t bytes = size * count;
    const auto line = QString::fromLatin1(data, static_cast<qsizetype>(bytes));
    const auto prefix = QStringLiteral("retry-after:");
    if (line.startsWith(prefix, Qt::CaseInsensitive)) {
        response->retry_after = line.mid(prefix.size()).trimmed().toStdString();
    }
    return bytes;
}

int check_cancel(void* context, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
    const auto& probe = *static_cast<const std::function<bool()>*>(context);
    // Never allow a caller's exception to unwind through cURL's C stack.
    try {
        return probe && probe() ? 1 : 0;
    } catch (...) {
        return 1;
    }
}

std::string assistant_text(const std::string& body)
{
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(QByteArray::fromStdString(body), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        throw std::runtime_error("Response Error: Expected a JSON chat-completion response.");
    }
    const auto choices = document.object().value("choices").toArray();
    const auto message = choices.isEmpty() ? QJsonObject() : choices.first().toObject().value("message").toObject();
    const auto content = message.value("content");
    QString text;
    if (content.isString()) {
        text = content.toString();
    } else if (content.isArray()) {
        for (const auto& part : content.toArray()) {
            const auto object = part.toObject();
            if (object.value("type").toString() == QStringLiteral("text")) {
                text += object.value("text").toString();
            }
        }
    }
    if (text.trimmed().isEmpty()) {
        throw std::runtime_error("Response Error: The API returned no assistant text. Check the selected model and output token limits.");
    }
    return text.toStdString();
}
} // namespace

ChatCompletionTransport::ChatCompletionTransport(std::string api_key, std::string base_url)
    : api_key_(std::move(api_key)), base_url_(std::move(base_url))
{}

std::string ChatCompletionTransport::complete(const std::string& payload,
                                             const std::function<bool()>& should_cancel) const
{
    if (should_cancel && should_cancel()) {
        throw ChatCompletionCancelled();
    }
    CurlRequest request;
    CURL* handle = request.handle.get();
    if (!handle) {
        throw std::runtime_error("Initialization Error: Failed to initialize cURL.");
    }
#ifdef _WIN32
    const auto cert_path = Utils::ensure_ca_bundle().string();
    curl_easy_setopt(handle, CURLOPT_CAINFO, cert_path.c_str());
#endif
    const auto url = completion_url(base_url_);
    Response response;
    request.headers = curl_slist_append(request.headers, "Content-Type: application/json");
    if (!api_key_.empty()) {
        request.headers = curl_slist_append(request.headers, ("Authorization: Bearer " + api_key_).c_str());
    }
    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle, CURLOPT_POST, 1L);
    curl_easy_setopt(handle, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(payload.size()));
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, request.headers);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, timeout_seconds(base_url_));
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, kConnectTimeoutSeconds);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, receive_body);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, receive_header);
    curl_easy_setopt(handle, CURLOPT_HEADERDATA, &response);
    curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(handle, CURLOPT_XFERINFOFUNCTION, check_cancel);
    curl_easy_setopt(handle, CURLOPT_XFERINFODATA, &should_cancel);

    const auto result = curl_easy_perform(handle);
    if (result == CURLE_ABORTED_BY_CALLBACK) {
        throw ChatCompletionCancelled();
    }
    if (result != CURLE_OK) {
        throw std::runtime_error("Network Error: " + std::string(curl_easy_strerror(result)));
    }
    long status = 0;
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status);
    if (status >= 400) {
        RemoteApiError::throw_for_http_error("Remote LLM", status, response.body, response.retry_after,
                                             Logger::get_logger("core_logger"));
    }
    return assistant_text(response.body);
}
