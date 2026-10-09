#include <catch2/catch_test_macros.hpp>

#include "ApiImageAnalyzer.hpp"
#include "BoundedWorkExecutor.hpp"
#include "ILLMClient.hpp"
#include "LLMClient.hpp"
#include "OpenAiVision.hpp"
#include "TestHelpers.hpp"

#if __has_include(<jsoncpp/json/json.h>)
    #include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
    #include <json/json.h>
#else
    #error "jsoncpp headers not found. Install jsoncpp development files."
#endif

#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QString>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

Json::Value parse_json(const std::string& text)
{
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::Value root;
    std::string errors;
    REQUIRE(reader->parse(text.data(), text.data() + text.size(), &root, &errors));
    return root;
}

bool starts_with(const std::string& value, const std::string& prefix)
{
    return value.rfind(prefix, 0) == 0;
}

std::string base64_payload(const std::string& data_url)
{
    const auto comma = data_url.find(',');
    REQUIRE(comma != std::string::npos);
    return data_url.substr(comma + 1);
}

QImage decode_data_url(const std::string& data_url)
{
    const QByteArray bytes = QByteArray::fromBase64(QByteArray::fromStdString(base64_payload(data_url)));
    return QImage::fromData(bytes);
}

// Fake client that records what it was asked and returns a scripted reply.
class ScriptedVisionLLM : public ILLMClient {
public:
    explicit ScriptedVisionLLM(std::string reply) : reply_(std::move(reply)) {}

    std::string categorize_file(const std::string&, const std::string&, FileType, const std::string&) override
    {
        return "Documents : Reports";
    }
    std::string complete_prompt(const std::string&, int) override
    {
        return "Documents : Reports";
    }
    void set_prompt_logging_enabled(bool) override {}

    bool supports_image_input() const override { return true; }
    std::string complete_prompt_with_image(const std::string& prompt,
                                           const std::string& image_path,
                                           const ImageCompletionOptions& options) override
    {
        last_prompt = prompt;
        last_image_path = image_path;
        last_max_tokens = options.max_tokens;
        last_temperature = options.temperature;
        last_response_format = options.response_format_json;
        return reply_;
    }

    std::string last_prompt;
    std::string last_image_path;
    int last_max_tokens{0};
    double last_temperature{-1.0};
    std::string last_response_format;

private:
    std::string reply_;
};

// Client that never claims image support, using the ILLMClient defaults.
class TextOnlyLLM : public ILLMClient {
public:
    std::string categorize_file(const std::string&, const std::string&, FileType, const std::string&) override
    {
        return "Documents : Reports";
    }
    std::string complete_prompt(const std::string&, int) override
    {
        return "Documents : Reports";
    }
    void set_prompt_logging_enabled(bool) override {}
};

} // namespace

TEST_CASE("OpenAI vision payload uses standard chat content parts") {
    const std::string data_url = "data:image/jpeg;base64,AAAA";
    const std::string payload = OpenAiVision::build_image_chat_payload(
        "gemma-4-12b-it-UD-Q6_K_XL",
        "system text",
        "describe this",
        data_url,
        256);
    const Json::Value root = parse_json(payload);

    REQUIRE(root["model"].asString() == "gemma-4-12b-it-UD-Q6_K_XL");
    REQUIRE(root["max_tokens"].asInt() == 256);
    REQUIRE(root["messages"].isArray());
    REQUIRE(root["messages"].size() == 2);
    REQUIRE(root["messages"][0]["role"].asString() == "system");
    REQUIRE(root["messages"][0]["content"].asString() == "system text");

    const Json::Value& user = root["messages"][1];
    REQUIRE(user["role"].asString() == "user");
    REQUIRE(user["content"].isArray());
    REQUIRE(user["content"].size() == 2);
    REQUIRE(user["content"][0]["type"].asString() == "text");
    REQUIRE(user["content"][0]["text"].asString() == "describe this");
    REQUIRE(user["content"][1]["type"].asString() == "image_url");
    REQUIRE(user["content"][1]["image_url"]["url"].asString() == data_url);
}

TEST_CASE("OpenAI vision payload omits max_tokens when not positive") {
    const Json::Value root = parse_json(OpenAiVision::build_image_chat_payload(
        "m", "s", "u", "data:image/png;base64,AAAA", 0));
    REQUIRE_FALSE(root.isMember("max_tokens"));
}

TEST_CASE("OpenAI vision preparation encodes opaque images as JPEG and alpha images as PNG") {
    TempDir dir;
    const auto jpg_path = dir.path() / "photo.jpg";
    const auto png_path = dir.path() / "screenshot.png";

    QImage opaque(64, 48, QImage::Format_RGB32);
    opaque.fill(QColor(200, 30, 30));
    REQUIRE(opaque.save(QString::fromStdString(jpg_path.string()), "JPG"));

    QImage with_alpha(64, 48, QImage::Format_ARGB32);
    with_alpha.fill(QColor(0, 0, 255, 128));
    REQUIRE(with_alpha.save(QString::fromStdString(png_path.string()), "PNG"));

    const auto jpeg = OpenAiVision::prepare_image_data_url(jpg_path);
    REQUIRE(jpeg.mime_type == "image/jpeg");
    REQUIRE(starts_with(jpeg.data_url, "data:image/jpeg;base64,"));

    const auto png = OpenAiVision::prepare_image_data_url(png_path);
    REQUIRE(png.mime_type == "image/png");
    REQUIRE(starts_with(png.data_url, "data:image/png;base64,"));
}

TEST_CASE("OpenAI vision preparation downscales large images and never upscales small ones") {
    TempDir dir;
    const auto large_path = dir.path() / "large.png";
    const auto small_path = dir.path() / "small.png";

    QImage large(4000, 1000, QImage::Format_RGB32);
    large.fill(QColor(10, 120, 40));
    REQUIRE(large.save(QString::fromStdString(large_path.string()), "PNG"));

    QImage small(64, 48, QImage::Format_RGB32);
    small.fill(QColor(90, 90, 90));
    REQUIRE(small.save(QString::fromStdString(small_path.string()), "PNG"));

    const auto downscaled = decode_data_url(OpenAiVision::prepare_image_data_url(large_path).data_url);
    REQUIRE_FALSE(downscaled.isNull());
    REQUIRE(downscaled.width() == OpenAiVision::kMaxImageLongEdgePixels);
    REQUIRE(downscaled.height() == 512);

    const auto unchanged = decode_data_url(OpenAiVision::prepare_image_data_url(small_path).data_url);
    REQUIRE(unchanged.width() == 64);
    REQUIRE(unchanged.height() == 48);
}

TEST_CASE("OpenAI vision preparation reports undecodable files as errors") {
    TempDir dir;
    const auto bogus = dir.path() / "notes.jpg";
    {
        std::ofstream out(bogus, std::ios::binary);
        out << "this is not an image";
    }
    REQUIRE_THROWS_AS(OpenAiVision::prepare_image_data_url(bogus), std::runtime_error);
}

TEST_CASE("LLM client advertises image input only where the transport supports it") {
    LLMClient client("not-needed", "gemma", "http://127.0.0.1:8888/v1");
    REQUIRE(client.supports_image_input());

    TextOnlyLLM text_only;
    REQUIRE_FALSE(text_only.supports_image_input());
    REQUIRE_THROWS_AS(text_only.complete_prompt_with_image("p", "x.jpg", ILLMClient::ImageCompletionOptions{}),
                      std::runtime_error);
}

TEST_CASE("Vision reply parser accepts plain JSON") {
    const auto reply = ApiImageAnalyzer::parse_vision_reply(
        R"({"description": "A red bicycle leaning on a wall.", "suggested_name": "red_bicycle"})");
    REQUIRE(reply.description == "A red bicycle leaning on a wall.");
    REQUIRE(reply.suggested_name == "red_bicycle");
}

TEST_CASE("Vision reply parser accepts a single fenced JSON block") {
    const auto reply = ApiImageAnalyzer::parse_vision_reply(
        "```json\n{\"description\": \"Receipt on a desk.\", \"suggested_name\": \"receipt_desk\"}\n```");
    REQUIRE(reply.description == "Receipt on a desk.");
    REQUIRE(reply.suggested_name == "receipt_desk");
}

TEST_CASE("Vision reply parser rejects malformed JSON and free text") {
    REQUIRE_THROWS_AS(ApiImageAnalyzer::parse_vision_reply("{\"description\": \"unterminated"), std::runtime_error);
    REQUIRE_THROWS_AS(ApiImageAnalyzer::parse_vision_reply("Just a sentence about a cat."), std::runtime_error);
    REQUIRE_THROWS_AS(ApiImageAnalyzer::parse_vision_reply("[\"not\", \"an\", \"object\"]"), std::runtime_error);
}

TEST_CASE("Vision reply parser tolerates a missing description but requires a suggested name") {
    const auto without_description =
        ApiImageAnalyzer::parse_vision_reply(R"({"suggested_name": "invoice_march"})");
    REQUIRE(without_description.description.empty());
    REQUIRE(without_description.suggested_name == "invoice_march");

    REQUIRE_THROWS_AS(ApiImageAnalyzer::parse_vision_reply(R"({"description": "Only a description."})"),
                      std::runtime_error);
    REQUIRE_THROWS_AS(ApiImageAnalyzer::parse_vision_reply(R"({"suggested_name": "   "})"),
                      std::runtime_error);
    REQUIRE_THROWS_AS(ApiImageAnalyzer::parse_vision_reply(R"({"suggested_name": 42})"),
                      std::runtime_error);
}

TEST_CASE("API image analysis maps a reply to description and a sanitized filename with the original extension") {
    ScriptedVisionLLM llm(R"({"description": "A tabby cat asleep on a sofa.", "suggested_name": "Tabby cat on a sofa"})");
    const auto result = ApiImageAnalyzer::analyze_image(llm, std::filesystem::path("C:/photos/IMG_0042.JPG"));

    REQUIRE(result.description == "A tabby cat asleep on a sofa.");
    REQUIRE_FALSE(result.suggested_name.empty());
    REQUIRE(result.suggested_name.find('/') == std::string::npos);
    REQUIRE(result.suggested_name.find('\\') == std::string::npos);
    REQUIRE(result.suggested_name.size() >= 4);
    REQUIRE(result.suggested_name.substr(result.suggested_name.size() - 4) == ".JPG");

    REQUIRE(llm.last_image_path == "C:/photos/IMG_0042.JPG");
    REQUIRE(llm.last_max_tokens == ApiImageAnalyzer::kVisionMaxTokens);
    REQUIRE(llm.last_prompt.find("suggested_name") != std::string::npos);
}

TEST_CASE("API image analysis fails per image when the reply has no usable name") {
    ScriptedVisionLLM llm(R"({"description": "Something visible."})");
    REQUIRE_THROWS_AS(ApiImageAnalyzer::analyze_image(llm, std::filesystem::path("scan.png")),
                      std::runtime_error);
}

TEST_CASE("Vision reply parser accepts a single JSON object surrounded by prose") {
    const auto reply = ApiImageAnalyzer::parse_vision_reply(
        "Sure, here it is: {\"description\": \"Brown dog on grass.\", \"suggested_name\": \"brown_dog\"} Hope that helps.");
    REQUIRE(reply.description == "Brown dog on grass.");
    REQUIRE(reply.suggested_name == "brown_dog");
}

TEST_CASE("Vision reply parser rejects prose with no JSON object and several JSON objects") {
    REQUIRE_THROWS_AS(ApiImageAnalyzer::parse_vision_reply("I think this is a photo of a beach."),
                      std::runtime_error);
    REQUIRE_THROWS_AS(ApiImageAnalyzer::parse_vision_reply(
                          R"({"suggested_name": "first"} and also {"suggested_name": "second"})"),
                      std::runtime_error);
}

TEST_CASE("Vision reply parser keeps braces inside JSON strings intact") {
    const auto reply = ApiImageAnalyzer::parse_vision_reply(
        R"(Result: {"description": "A sign reading {OPEN}", "suggested_name": "open_sign"})");
    REQUIRE(reply.description == "A sign reading {OPEN}");
    REQUIRE(reply.suggested_name == "open_sign");
}

TEST_CASE("Message extraction returns final content and ignores reasoning when content is present") {
    const auto message = OpenAiVision::extract_assistant_message(
        R"({"choices": [{"message": {"role": "assistant",
            "content": "{\"description\": \"d\", \"suggested_name\": \"n\"}",
            "reasoning_content": "thinking about the image"}}]})");
    REQUIRE(message.content == R"({"description": "d", "suggested_name": "n"})");
    REQUIRE(message.reasoning_content == "thinking about the image");
}

TEST_CASE("Message extraction reports empty content with a useful diagnostic") {
    try {
        (void)OpenAiVision::extract_assistant_message(
            R"({"choices": [{"message": {"role": "assistant", "content": ""}}]})");
        FAIL("empty content should throw");
    } catch (const std::runtime_error& ex) {
        REQUIRE(std::string(ex.what()).find("empty content and no reasoning_content") != std::string::npos);
    }
}

TEST_CASE("Message extraction detects reasoning-only replies and says so") {
    try {
        (void)OpenAiVision::extract_assistant_message(
            R"({"choices": [{"message": {"role": "assistant", "content": null,
                "reasoning_content": "The image shows a receipt. Name: receipt_01"}}]})");
        FAIL("reasoning-only reply should throw");
    } catch (const std::runtime_error& ex) {
        const std::string message = ex.what();
        REQUIRE(message.find("reasoning_content") != std::string::npos);
        REQUIRE(message.find("reasoning output is on") != std::string::npos);
    }
}

TEST_CASE("Message extraction rejects bodies that are not chat completions") {
    REQUIRE_THROWS_AS(OpenAiVision::extract_assistant_message("<html>502</html>"), std::runtime_error);
    REQUIRE_THROWS_AS(OpenAiVision::extract_assistant_message(R"({"choices": []})"), std::runtime_error);
}

TEST_CASE("Image payload carries deterministic temperature and the structured response format") {
    const std::string format = ApiImageAnalyzer::build_image_analysis_response_format();
    const Json::Value root = parse_json(OpenAiVision::build_image_chat_payload(
        "m", "s", "u", "data:image/jpeg;base64,AAAA", 512, 0.0, format));

    REQUIRE(root["temperature"].asDouble() == 0.0);
    REQUIRE(root["max_tokens"].asInt() == 512);
    REQUIRE(root["response_format"]["type"].asString() == "json_schema");
    REQUIRE(root["response_format"]["json_schema"]["name"].asString() == "image_analysis");
    REQUIRE(root["response_format"]["json_schema"]["strict"].asBool());
    const Json::Value& schema = root["response_format"]["json_schema"]["schema"];
    REQUIRE(schema["required"].size() == 2);
    REQUIRE_FALSE(schema["additionalProperties"].asBool());
}

TEST_CASE("API image analysis sends deterministic settings and the structured format") {
    ScriptedVisionLLM llm(R"({"description": "A bird.", "suggested_name": "small_bird"})");
    (void)ApiImageAnalyzer::analyze_image(llm, std::filesystem::path("bird.png"));

    REQUIRE(llm.last_temperature == 0.0);
    REQUIRE(llm.last_max_tokens == ApiImageAnalyzer::kVisionMaxTokens);
    REQUIRE(llm.last_response_format.find("json_schema") != std::string::npos);
    REQUIRE(llm.last_response_format.find("suggested_name") != std::string::npos);
}

TEST_CASE("Four concurrent vision requests each return their own analysis") {
    std::vector<std::unique_ptr<ILLMClient>> clients;
    for (int i = 0; i < 4; ++i) {
        clients.push_back(std::make_unique<ScriptedVisionLLM>(
            R"({"description": "d", "suggested_name": "image_name"})"));
    }
    std::vector<std::string> inputs = {"a.jpg", "b.jpg", "c.jpg", "d.jpg", "e.jpg", "f.jpg", "g.jpg", "h.jpg"};
    const std::atomic<bool> stop{false};

    const auto outcomes = BoundedWorkExecutor::run_batch(
        inputs,
        clients,
        stop,
        [](std::unique_ptr<ILLMClient>& client, size_t, const std::string& path) {
            return ApiImageAnalyzer::analyze_image(*client, std::filesystem::path(path));
        });

    REQUIRE(outcomes.size() == inputs.size());
    for (size_t i = 0; i < outcomes.size(); ++i) {
        REQUIRE(outcomes[i].attempted());
        REQUIRE_FALSE(outcomes[i].error);
        // The sanitizer drops stopwords such as "image", so only the extension is fixed here.
        REQUIRE(outcomes[i].value->suggested_name.size() > 4);
        REQUIRE(outcomes[i].value->suggested_name.substr(outcomes[i].value->suggested_name.size() - 4)
                == inputs[i].substr(inputs[i].size() - 4));
    }
}
