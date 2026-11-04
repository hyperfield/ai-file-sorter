#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Settings.hpp"
#include "TestHelpers.hpp"
#include "LLMClient.hpp"
#include "GeminiClient.hpp"
#include "CustomApiDialog.hpp"
#include "VisualModelCatalog.hpp"
#include <QCheckBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QAbstractButton>
#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>

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
    endpoint.image_analysis_enabled = true;

    const std::string id = settings.upsert_custom_api_endpoint(endpoint);
    REQUIRE_FALSE(id.empty());
    settings.set_active_custom_api_id(id);
    settings.set_visual_model_id(http_visual_model_id(id));
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
    CHECK(loaded.image_analysis_enabled);
    CHECK(reloaded.get_visual_model_id() == http_visual_model_id(id));
    REQUIRE(reloaded.get_active_custom_api_id() == id);
}

TEST_CASE("Existing API connections require explicit opt-in for image uploads", "[http-vision]")
{
    TempDir config_dir;
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", config_dir.path().string());
    Settings settings;
    const auto path = std::filesystem::path(settings.get_config_dir()) / "config.ini";
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path) << "[CustomApis]\nCustomApiIds=old\n"
        "[CustomApi_old]\nName=Old connection\nBaseUrl=http://127.0.0.1:1234/v1\nModel=old-model\n";
    settings.load();
    const auto endpoint = settings.find_custom_api_endpoint("old");
    REQUIRE(endpoint.id == "old");
    CHECK_FALSE(endpoint.image_analysis_enabled);
}

TEST_CASE("Custom API dialog edits vision capability explicitly", "[http-vision]")
{
    QtAppContext qt;
    CustomApiEndpoint endpoint{"vision", "My server", "", "http://127.0.0.1:1234/v1", "", "vision-model", true};
    CustomApiDialog editor(nullptr, endpoint);
    auto* checkbox = editor.findChild<QCheckBox*>(QStringLiteral("apiVisionCapability"));
    REQUIRE(checkbox);
    CHECK(checkbox->isChecked());
    CHECK(editor.result().image_analysis_enabled);
    checkbox->setChecked(false);
    CHECK_FALSE(editor.result().image_analysis_enabled);
    CustomApiDialog fresh;
    CHECK_FALSE(fresh.result().image_analysis_enabled);
}

TEST_CASE("Enabling API vision asks for upload confirmation and honors OK and Cancel", "[http-vision][image-upload-confirmation]")
{
    QtAppContext qt;
    CustomApiDialog editor;
    auto* checkbox = editor.findChild<QCheckBox*>(QStringLiteral("apiVisionCapability"));
    REQUIRE(checkbox);
    const bool accept = GENERATE(false, true);
    int confirmations = 0;
    QTimer dismiss;
    QObject::connect(&dismiss, &QTimer::timeout, &editor, [&] {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (!box) return;
        dismiss.stop();
        ++confirmations;
        CHECK(box->parentWidget() == &editor);
        CHECK(box->objectName() == QStringLiteral("imageUploadConfirmation"));
        CHECK(box->icon() == QMessageBox::Information);
        CHECK(box->text().contains(QStringLiteral("scanned images will be sent")));
        CHECK(box->text().contains(QStringLiteral("configured API server")));
        CHECK(box->standardButtons() == (QMessageBox::Ok | QMessageBox::Cancel));
        CHECK(box->defaultButton() == box->button(QMessageBox::Cancel));
        box->button(accept ? QMessageBox::Ok : QMessageBox::Cancel)->click();
    });
    dismiss.start(0);
    checkbox->click();
    CHECK(confirmations == 1);
    CHECK(checkbox->isChecked() == accept);
    CHECK(editor.result().image_analysis_enabled == accept);
    if (accept) {
        checkbox->click();
        CHECK_FALSE(checkbox->isChecked());
        CHECK(confirmations == 1);
    }
    dismiss.start(0);
    checkbox->click();
    CHECK(confirmations == 2);
    CHECK(editor.result().image_analysis_enabled == accept);
}
