#include <catch2/catch_test_macros.hpp>

#include "Settings.hpp"
#include "TestHelpers.hpp"
#include "LLMClient.hpp"
#include "GeminiClient.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

struct APICompletionTestAccess {
    static std::string payload(LLMClient& client, const std::string& prompt) {
        return client.make_completion_payload(prompt, 91);
    }
    static std::string payload(GeminiClient& client, const std::string& prompt) {
        return client.make_completion_payload(prompt, 91);
    }
};

TEST_CASE("API completions preserve each task's response format without a JSON persona", "[prompt-contract]")
{
    LLMClient openai("unused-test-key", "test-model");
    GeminiClient gemini("unused-test-key", "test-model");
    for (const std::string task : {"Return only the translated filename stem; no JSON.",
                                  "Return JSON with summary and filename.",
                                  "Return one line per item, followed by END."}) {
        const auto first = QJsonDocument::fromJson(QByteArray::fromStdString(APICompletionTestAccess::payload(openai, task))).object();
        const auto messages = first.value("messages").toArray();
        REQUIRE(messages.size() == 2);
        CHECK(messages[0].toObject().value("content").toString().toStdString() == LLMPrompt::kCompletionSystemPrompt);
        CHECK(messages[1].toObject().value("content").toString().toStdString() == task);
        CHECK(first.value("max_tokens").toInt() == 91);
        const auto second = QJsonDocument::fromJson(QByteArray::fromStdString(APICompletionTestAccess::payload(gemini, task))).object();
        const auto merged = second.value("contents").toArray()[0].toObject().value("parts").toArray()[0]
            .toObject().value("text").toString().toStdString();
        CHECK(merged == std::string(LLMPrompt::kCompletionSystemPrompt) + "\n\n" + task);
        CHECK(second.value("generationConfig").toObject().value("maxOutputTokens").toInt() == 91);
    }
}

TEST_CASE("Custom API endpoints persist across Settings load/save") {
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());

    Settings settings;
    settings.load();

    CustomApiEndpoint endpoint;
    endpoint.name = "Local LM Studio";
    endpoint.description = "Local OpenAI-compatible endpoint";
    endpoint.base_url = "http://localhost:1234/v1";
    endpoint.api_key = "local-key";
    endpoint.model = "llama-3.1";

    const std::string id = settings.upsert_custom_api_endpoint(endpoint);
    REQUIRE_FALSE(id.empty());
    settings.set_active_custom_api_id(id);
    REQUIRE(settings.save());

    Settings reloaded;
    reloaded.load();
    const CustomApiEndpoint loaded = reloaded.find_custom_api_endpoint(id);

    REQUIRE(loaded.id == id);
    REQUIRE(loaded.name == endpoint.name);
    REQUIRE(loaded.description == endpoint.description);
    REQUIRE(loaded.base_url == endpoint.base_url);
    REQUIRE(loaded.api_key == endpoint.api_key);
    REQUIRE(loaded.model == endpoint.model);
    REQUIRE(reloaded.get_active_custom_api_id() == id);
}
