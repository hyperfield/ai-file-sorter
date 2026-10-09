#include "OpenAiVision.hpp"

#include "ImageDecodeUtils.hpp"
#include "Logger.hpp"
#include "Utils.hpp"

#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QSize>
#include <QString>

#if __has_include(<jsoncpp/json/json.h>)
    #include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
    #include <json/json.h>
#else
    #error "jsoncpp headers not found. Install jsoncpp development files."
#endif

#include <memory>
#include <stdexcept>

namespace OpenAiVision {
namespace {

QImage decode_bounded(const std::filesystem::path& image_path)
{
    const QString path = QString::fromStdString(Utils::path_to_utf8(image_path));

    // Only request a bounded decode when the source is actually larger than the limit.
    // ImageDecodeUtils scales to fit, which would also enlarge a small image.
    QImageReader probe(path);
    probe.setAutoTransform(true);
    const QSize source_size = probe.size();
    QSize max_size;
    if (source_size.isValid() &&
        (source_size.width() > kMaxImageLongEdgePixels || source_size.height() > kMaxImageLongEdgePixels)) {
        max_size = QSize(kMaxImageLongEdgePixels, kMaxImageLongEdgePixels);
    }

    std::string error;
    QImage image = ImageDecodeUtils::decode_image_with_webp_fallback(path, max_size, &error);
    if (image.isNull()) {
        throw std::runtime_error("Failed to decode image" + (error.empty() ? std::string() : ": " + error));
    }
    return image;
}

// Builds {"role":"user","content":[text part, image part]}; the text part comes first.
Json::Value make_user_message(const std::string& user_text, const std::string& image_data_url)
{
    Json::Value text_part(Json::objectValue);
    text_part["type"] = "text";
    text_part["text"] = user_text;

    Json::Value image_url(Json::objectValue);
    image_url["url"] = image_data_url;
    Json::Value image_part(Json::objectValue);
    image_part["type"] = "image_url";
    image_part["image_url"] = image_url;

    Json::Value user_content(Json::arrayValue);
    user_content.append(text_part);
    user_content.append(image_part);
    Json::Value user_message(Json::objectValue);
    user_message["role"] = "user";
    user_message["content"] = user_content;
    return user_message;
}

Json::Value parse_response_format(const std::string& response_format_json)
{
    Json::CharReaderBuilder reader_builder;
    std::unique_ptr<Json::CharReader> reader(reader_builder.newCharReader());
    Json::Value response_format;
    std::string errors;
    if (!reader->parse(response_format_json.data(),
                       response_format_json.data() + response_format_json.size(),
                       &response_format,
                       &errors)) {
        throw std::runtime_error("Invalid response_format JSON: " + errors);
    }
    return response_format;
}

} // namespace

PreparedImage prepare_image_data_url(const std::filesystem::path& image_path)
{
    const QImage image = decode_bounded(image_path);

    const bool has_alpha = image.hasAlphaChannel();
    QByteArray encoded;
    QBuffer buffer(&encoded);
    if (!buffer.open(QIODevice::WriteOnly)) {
        throw std::runtime_error("Failed to open buffer for image encoding");
    }

    QImageWriter writer(&buffer, has_alpha ? "PNG" : "JPEG");
    if (!has_alpha) {
        writer.setQuality(kJpegQuality);
    }
    if (!writer.write(image)) {
        throw std::runtime_error("Failed to encode image: " + writer.errorString().toStdString());
    }
    buffer.close();

    // Base64 is produced exactly once; the raw bytes are released when this function returns.
    const QByteArray base64 = encoded.toBase64();
    PreparedImage prepared;
    prepared.mime_type = has_alpha ? "image/png" : "image/jpeg";
    prepared.data_url = "data:" + prepared.mime_type + ";base64,"
                      + std::string(base64.constData(), static_cast<size_t>(base64.size()));
    return prepared;
}

std::string build_image_chat_payload(const std::string& model,
                                     const std::string& system_prompt,
                                     const std::string& user_text,
                                     const std::string& image_data_url,
                                     int max_tokens,
                                     double temperature,
                                     const std::string& response_format_json)
{
    Json::Value system_message(Json::objectValue);
    system_message["role"] = "system";
    system_message["content"] = system_prompt;

    Json::Value messages(Json::arrayValue);
    messages.append(system_message);
    messages.append(make_user_message(user_text, image_data_url));

    Json::Value root(Json::objectValue);
    root["model"] = model;
    root["messages"] = messages;
    root["temperature"] = temperature;
    if (max_tokens > 0) {
        root["max_tokens"] = max_tokens;
    }
    if (!response_format_json.empty()) {
        root["response_format"] = parse_response_format(response_format_json);
    }

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, root);
}

AssistantMessage extract_assistant_message(const std::string& response_body)
{
    Json::CharReaderBuilder reader_builder;
    std::unique_ptr<Json::CharReader> reader(reader_builder.newCharReader());
    Json::Value root;
    std::string errors;
    if (!reader->parse(response_body.data(), response_body.data() + response_body.size(), &root, &errors)) {
        throw std::runtime_error("Vision response is not valid JSON");
    }

    const Json::Value& choices = root["choices"];
    if (!choices.isArray() || choices.empty()) {
        throw std::runtime_error("Vision response has no choices");
    }
    const Json::Value& message = choices[0]["message"];
    if (!message.isObject()) {
        throw std::runtime_error("Vision response choices[0] has no message object");
    }

    AssistantMessage result;
    if (message["content"].isString()) {
        result.content = message["content"].asString();
    }
    if (message["reasoning_content"].isString()) {
        result.reasoning_content = message["reasoning_content"].asString();
    }

    const bool content_blank = result.content.find_first_not_of(" \t\r\n") == std::string::npos;
    if (!content_blank) {
        return result;
    }

    // Safe to log: this is the assistant's own reply, never the request (no image, no API key).
    Json::StreamWriterBuilder writer_builder;
    writer_builder["indentation"] = "";
    std::string raw_message = Json::writeString(writer_builder, message);
    constexpr size_t kMaxLoggedChars = 1000;
    if (raw_message.size() > kMaxLoggedChars) {
        raw_message.resize(kMaxLoggedChars);
    }
    if (auto logger = Logger::get_logger("core_logger")) {
        logger->warn("[VISION-DEBUG] Empty assistant content; choices[0].message: {}", raw_message);
    }

    const bool has_reasoning = result.reasoning_content.find_first_not_of(" \t\r\n") != std::string::npos;
    if (has_reasoning) {
        throw std::runtime_error(
            "Vision reply has empty content and the answer is in reasoning_content. The server returned "
            "reasoning instead of the final answer (reasoning output is on); check the server's reasoning setting.");
    }
    throw std::runtime_error("Vision reply has empty content and no reasoning_content.");
}

} // namespace OpenAiVision
