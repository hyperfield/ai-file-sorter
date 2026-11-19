#include "ChatCompletionTestServer.hpp"
#include "ChatCompletionTransport.hpp"
#include "ContentAnalysisUnavailable.hpp"
#include "HttpImageAnalyzer.hpp"
#include "HeadlessAnalysisWorkflowHost.hpp"
#include "ImageAnalyzerFactory.hpp"
#include "LLMClient.hpp"
#include "LLMErrors.hpp"
#include "Settings.hpp"
#include "TestHelpers.hpp"
#include "VisualLlmRuntime.hpp"
#include "Utils.hpp"

#include <QJsonArray>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <atomic>
#include <fstream>

namespace {
CustomApiEndpoint connection(const ChatCompletionTestServer& server)
{
    return {"vision-test", "Local vision server", "", server.url(), "", "vision-model", true};
}

std::filesystem::path write_image(TempDir& temp)
{
    const auto path = temp.path() / Utils::utf8_to_path("café.PNG");
    QImage image(2000, 1000, QImage::Format_RGB32);
    image.fill(Qt::red);
    image.setText(QStringLiteral("Comment"), QStringLiteral("Private image metadata"));
    if (!image.save(QString::fromStdString(Utils::path_to_utf8(path)), "PNG")) {
        throw std::runtime_error("Cannot write test image.");
    }
    return path;
}
} // namespace

TEST_CASE("HTTP vision uploads image pixels and requests a safe filename without local models", "[http-vision]")
{
    QtAppContext qt;
    TempDir temp;
    ChatCompletionTestServer server;
    auto endpoint = connection(server);
    const bool full_url = GENERATE(false, true);
    endpoint.base_url += full_url ? "/chat/completions/" : "/";
    endpoint.api_key = "vision-test-token";
    const auto backend = VisualLlmRuntime::resolve_active_backend(http_visual_model_id(endpoint.id), {}, {endpoint});
    REQUIRE(backend);
    REQUIRE(backend->artifacts.empty());
    auto analyzer = ImageAnalyzerFactory::create(*backend);
    server.respond = [&](const auto&) {
        return ChatCompletionTestServer::answer(server.requests.size() == 1
            ? QStringLiteral("A red boat crosses a quiet lake beside a forest.")
            : QStringLiteral("Filename: \"Red / Boat Boat : Lake Sunset.png\"\nextra text"));
    };
    const auto path = write_image(temp);
    const auto result = server.run([&] { return analyzer->analyze(path); });
    CHECK(result.description == "A red boat crosses a quiet lake beside a forest.");
    CHECK(result.suggested_name == "red_boat_lake.PNG");
    REQUIRE(server.requests.size() == 2);
    for (const auto& request : server.requests) {
        CHECK(request.headers.startsWith("POST /v1/chat/completions HTTP/"));
        CHECK(request.headers.contains("Authorization: Bearer vision-test-token"));
        CHECK(request.json.value("model").toString() == QStringLiteral("vision-model"));
        CHECK_FALSE(request.json.value("stream").toBool(true));
        CHECK_FALSE(request.json.contains("response_format"));
    }
    const auto first = server.requests[0].json.value("messages").toArray()[0].toObject().value("content").toArray();
    REQUIRE(first.size() == 2);
    const auto data = first[1].toObject().value("image_url").toObject().value("url").toString();
    const auto prefix = QStringLiteral("data:image/png;base64,");
    REQUIRE(data.startsWith(prefix));
    const auto image = QImage::fromData(QByteArray::fromBase64(data.mid(prefix.size()).toLatin1()));
    REQUIRE_FALSE(image.isNull());
    CHECK(image.width() == 1536);
    CHECK(image.height() == 768);
    CHECK(image.pixelColor(0, 0) == QColor(Qt::red));
    CHECK(image.textKeys().isEmpty());
    const auto second = server.requests[1].json.value("messages").toArray()[0].toObject().value("content");
    REQUIRE(second.isString());
    CHECK(second.toString().contains(QString::fromStdString(result.description)));
    CHECK_FALSE(second.toString().contains("data:image"));
}

TEST_CASE("HTTP vision derives a filename from its description when the filename request fails", "[http-vision]")
{
    QtAppContext qt;
    TempDir temp;
    ChatCompletionTestServer server;
    server.respond = [&](const auto&) -> ChatCompletionTestServer::Reply {
        return server.requests.size() == 1 ? ChatCompletionTestServer::answer(QStringLiteral("Red boat crosses quiet lake."))
            : ChatCompletionTestServer::Reply{503, R"({"error":{"message":"temporarily unavailable"}})"};
    };
    ImageAnalyzerSettings settings;
    settings.max_filename_words = 6;
    HttpImageAnalyzer analyzer(connection(server), settings);
    const auto path = write_image(temp);
    const auto result = server.run([&] { return analyzer.analyze(path); });
    CHECK(result.description == "Red boat crosses quiet lake.");
    CHECK(result.suggested_name == "red_boat_crosses_quiet_lake.PNG");
    CHECK_FALSE(server.requests[0].headers.contains("Authorization:"));
}

TEST_CASE("HTTP vision rejects unusable responses and preserves server diagnostics", "[http-vision]")
{
    QtAppContext qt;
    TempDir temp;
    ChatCompletionTestServer server;
    const auto reply = GENERATE(
        ChatCompletionTestServer::Reply{401, R"({"error":{"message":"invalid API key"}})"},
        ChatCompletionTestServer::Reply{400, R"({"error":{"message":"image input is not supported by this model"}})"},
        ChatCompletionTestServer::Reply{200, "not JSON"},
        ChatCompletionTestServer::Reply{200, R"({"choices":[{"message":{"content":null,"reasoning_content":"thinking"}}]})"},
        ChatCompletionTestServer::Reply{200, R"({"choices":[]})"});
    server.respond = [&](const auto&) { return reply; };
    HttpImageAnalyzer analyzer(connection(server));
    const auto path = write_image(temp);
    try {
        server.run([&] { return analyzer.analyze(path); });
        FAIL("Expected API error");
    } catch (const ContentAnalysisUnavailable&) {
        FAIL("API failure must not be recorded as an unreadable image");
    } catch (const std::runtime_error& ex) {
        if (reply.status == 401) CHECK(std::string(ex.what()).find("invalid API key") != std::string::npos);
        if (reply.status == 400) CHECK(std::string(ex.what()).find("image input is not supported") != std::string::npos);
    }
    REQUIRE(server.requests.size() == 1);
}

TEST_CASE("HTTP vision cancels an in-flight request and never sends a filename request", "[http-vision]")
{
    QtAppContext qt;
    TempDir temp;
    ChatCompletionTestServer server;
    std::atomic<bool> stop{false};
    server.respond = [&](const auto&) -> std::optional<ChatCompletionTestServer::Reply> {
        stop = true;
        return std::nullopt;
    };
    ImageAnalyzerSettings settings;
    settings.should_cancel = [&] { return stop.load(); };
    HttpImageAnalyzer analyzer(connection(server), settings);
    const auto path = write_image(temp);
    CHECK_THROWS_AS(server.run([&] { return analyzer.analyze(path); }), ChatCompletionCancelled);
    REQUIRE(server.requests.size() == 1);
}

TEST_CASE("HTTP vision fails unreadable images before contacting the server", "[http-vision]")
{
    QtAppContext qt;
    TempDir temp;
    ChatCompletionTestServer server;
    const auto path = temp.path() / "broken.png";
    std::ofstream(path) << "not an image";
    HttpImageAnalyzer analyzer(connection(server));
    CHECK_THROWS_AS(analyzer.analyze(path), ContentAnalysisUnavailable);
    CHECK(server.requests.empty());
}

TEST_CASE("Text and vision clients share chat transport and accept typed text responses", "[http-vision][chat-transport]")
{
    QtAppContext qt;
    ChatCompletionTestServer server;
    server.respond = [](const auto&) -> ChatCompletionTestServer::Reply {
        return {200, R"({"choices":[{"message":{"content":[{"type":"text","text":"reply "},{"type":"text","text":"text"}]}}]})"};
    };
    LLMClient client("", "text-model", server.url());
    CHECK(server.run([&] { return client.complete_prompt("Return plain text.", 100); }) == "reply text");
    REQUIRE(server.requests.size() == 1);
    CHECK(server.requests[0].json.value("model").toString() == QStringLiteral("text-model"));
}

TEST_CASE("Chat transport preserves Retry-After and obeys custom endpoint timeouts", "[http-vision][chat-transport]")
{
    QtAppContext qt;
    ChatCompletionTestServer server;
    ChatCompletionTransport transport("", server.url());
    SECTION("rate limit") {
        server.respond = [](const auto&) -> ChatCompletionTestServer::Reply {
            return {429, R"({"error":{"message":"Too many requests"}})", "Retry-After: 17\r\n"};
        };
        try {
            server.run([&] { return transport.complete("{}"); });
            FAIL("Expected rate-limit error");
        } catch (const BackoffError& ex) {
            CHECK(ex.retry_after_seconds() == 17);
        }
    }
    SECTION("timeout") {
        EnvVarGuard timeout("AI_FILE_SORTER_CUSTOM_LLM_TIMEOUT", "1");
        server.respond = [](const auto&) -> std::optional<ChatCompletionTestServer::Reply> { return std::nullopt; };
        CHECK_THROWS_AS(server.run([&] { return transport.complete("{}"); }), std::runtime_error);
    }
}

TEST_CASE("Headless image renaming uses the persisted vision API and supports external cancellation", "[http-vision][headless]")
{
    QtAppContext qt;
    TempDir config, source;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config.path().string());
    Settings settings;
    ChatCompletionTestServer server;
    const auto id = settings.upsert_custom_api_endpoint(connection(server));
    settings.set_visual_model_id(http_visual_model_id(id));
    settings.set_analyze_images_by_content(true);
    settings.set_analyze_documents_by_content(false);
    settings.set_add_image_date_place_to_filename(false);
    REQUIRE(settings.save());
    const auto path = write_image(source);
    const bool cancel = GENERATE(false, true);
    std::atomic<bool> stop{false};
    server.respond = [&](const auto& request) -> std::optional<ChatCompletionTestServer::Reply> {
        if (cancel) {
            stop = true;
            return std::nullopt;
        }
        const auto content = request.json.value("messages").toArray()[0].toObject().value("content");
        return ChatCompletionTestServer::answer(content.isArray()
            ? QStringLiteral("A red boat crosses a quiet lake.") : QStringLiteral("red_boat_lake"));
    };
    HeadlessAnalysisWorkflowHost::Options options;
    options.folder_path = source.path();
    options.operation_mode = HeadlessAnalysisWorkflowHost::OperationMode::Rename;
    options.stop_requested = [&] { return stop.load(); };
    HeadlessAnalysisWorkflowHost host(options);
    const auto result = server.run([&] { return host.execute(); });
    INFO(result.error_message);
    if (cancel) {
        CHECK(result.status == AnalysisRunStatus::Cancelled);
        CHECK(host.review_entries().empty());
        CHECK(server.requests.size() == 1);
    } else {
        REQUIRE(result.status == AnalysisRunStatus::Completed);
        REQUIRE(host.review_entries().size() == 1);
        CHECK(host.review_entries()[0].suggested_name == "red_boat_lake.PNG");
        CHECK(server.requests.size() == 2);
    }
    CHECK(std::filesystem::exists(path));
}
