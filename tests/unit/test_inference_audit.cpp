#include "InferenceAudit.hpp"
#include "TestHelpers.hpp"
#include <QFile>
#include <QJsonDocument>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <array>

namespace {
constexpr char kAuditEnv[] = "AI_FILE_SORTER_INFERENCE_AUDIT_DIR";
const std::array<std::int32_t, 4> kTokens{2, 17, 23, 29};

class PrivateAuditRoot : public TempDir {
public:
    PrivateAuditRoot()
    {
#ifndef _WIN32
        std::filesystem::permissions(path(), std::filesystem::perms::owner_all);
#endif
    }
};

QJsonObject read_capture(const std::filesystem::path& root, const std::string& id, int attempt = 1)
{
    QFile file(QString::fromStdString((root / id / ("attempt-" + std::to_string(attempt) + ".json")).string()));
    REQUIRE(file.open(QIODevice::ReadOnly));
    const auto document = QJsonDocument::fromJson(file.readAll());
    REQUIRE(document.isObject());
    return document.object();
}

std::unique_ptr<InferenceAudit> create_audit(std::string& id, int attempt = 1)
{
    auto audit = InferenceAudit::from_environment("model.gguf", "system", "original evidence", "template", id, attempt);
    audit->set_runtime({32, 24, 8, 2, 1234, 0.8f, 0.05f, "cpu"});
    return audit;
}
}

TEST_CASE("InferenceAudit is disabled without explicit opt-in", "[inference-audit]")
{
    EnvVarGuard guard(kAuditEnv, "");
    std::string id;
    CHECK_FALSE(InferenceAudit::from_environment("model", "system", "private data", "", id, 1));
    CHECK(id.empty());
}

TEST_CASE("InferenceAudit captures fitting and exact successful decode chunks", "[inference-audit]")
{
    PrivateAuditRoot root;
    EnvVarGuard guard(kAuditEnv, root.path().string());
    std::string id;
    auto audit = create_audit(id);
    const std::array<std::int32_t, 25> oversized{};
    audit->record_fit("original evidence", "system original evidence", oversized, "system original evidence");
    audit->record_fit("short evidence", "system short evidence", kTokens, "<bos>system short evidence");
    audit->submit_prompt(0, std::span(kTokens).first(2));
    CHECK_THROWS_AS(audit->submit_prompt(0, kTokens), InferenceAudit::Error);
    auto checkpoint = read_capture(root.path(), id);
    CHECK(checkpoint["status"] == "started");
    CHECK_FALSE(checkpoint["allPromptTokensAcceptedExactly"].toBool());
    CHECK(checkpoint["promptDecodeCalls"].toArray().first().toObject()["status"] == "submitted");
    audit->prompt_result(0);
    audit->submit_prompt(2, std::span(kTokens).subspan(2));
    audit->prompt_result(0);
    audit->record_generation("  raw reply\n", "end-of-generation-token", 3);
    audit->finish("raw reply");
    const auto capture = read_capture(root.path(), id);
    CHECK(capture["status"] == "finished");
    CHECK(capture["allPromptTokensAcceptedExactly"].toBool());
    CHECK(capture["acceptedPromptTokens"] == 4);
    CHECK(capture["selectedFitAttempt"] == 1);
    const auto fits = capture["fitAttempts"].toArray();
    REQUIRE(fits.size() == 2);
    CHECK_FALSE(fits[0].toObject()["withinBudget"].toBool());
    CHECK(fits[1].toObject()["withinBudget"].toBool());
    CHECK(fits[1].toObject()["userPromptChanged"].toBool());
    CHECK(fits[1].toObject()["formattedTextPresentInTokens"].toBool());
    CHECK(capture["generation"].toObject()["rawResponse"].toObject()["text"] == "  raw reply\n");
    CHECK(capture["returnedResponse"].toObject()["text"] == "raw reply");
#ifndef _WIN32
    const auto directory_mode = std::filesystem::status(root.path() / id).permissions();
    CHECK((directory_mode & std::filesystem::perms::all) == std::filesystem::perms::owner_all);
    const auto file_mode = std::filesystem::status(root.path() / id / "attempt-1.json").permissions();
    CHECK((file_mode & std::filesystem::perms::all) == (std::filesystem::perms::owner_read | std::filesystem::perms::owner_write));
#endif
}

TEST_CASE("InferenceAudit rejects false completeness on failed or altered input", "[inference-audit]")
{
    const int problem = GENERATE(0, 1, 2);
    PrivateAuditRoot root;
    EnvVarGuard guard(kAuditEnv, root.path().string());
    std::string id;
    auto audit = create_audit(id);
    audit->record_fit("original evidence", "system original evidence", kTokens, "system original evidence");
    if (problem == 0) {
        audit->submit_prompt(0, kTokens);
        audit->prompt_result(-1);
    } else if (problem == 1) {
        auto altered = kTokens;
        altered[0] = 999;
        audit->submit_prompt(0, altered);
        audit->prompt_result(0);
    } else {
        audit->submit_prompt(2, std::span(kTokens).subspan(2));
        audit->prompt_result(0);
    }
    audit->finish("");
    CHECK_FALSE(read_capture(root.path(), id)["allPromptTokensAcceptedExactly"].toBool());
}

TEST_CASE("InferenceAudit preserves unfinished requests and byte-exact payloads", "[inference-audit]")
{
    PrivateAuditRoot root;
    EnvVarGuard guard(kAuditEnv, root.path().string());
    std::string id;
    const std::string payload("text\0\xff", 6);
    {
        auto audit = InferenceAudit::from_environment("model", "system", payload, "template", id, 1);
        audit->record_fit(payload, payload, kTokens, payload);
        audit->submit_prompt(0, kTokens);
    }
    const auto capture = read_capture(root.path(), id);
    CHECK(capture["status"] == "incomplete");
    CHECK_FALSE(capture["allPromptTokensAcceptedExactly"].toBool());
    const auto bytes = capture["originalUserPrompt"].toObject();
    const auto decoded = QByteArray::fromBase64(bytes["base64"].toString().toLatin1());
    CHECK(decoded == QByteArray(payload.data(), static_cast<qsizetype>(payload.size())));
    CHECK(bytes["bytes"] == 6);
}

TEST_CASE("InferenceAudit keeps failed attempts separate from retries", "[inference-audit]")
{
    PrivateAuditRoot root;
    EnvVarGuard guard(kAuditEnv, root.path().string());
    std::string id;
    {
        auto audit = create_audit(id);
        audit->fail("budget exceeded");
    }
    {
        auto retry = create_audit(id, 2);
        retry->finish("retry response");
    }
    CHECK(read_capture(root.path(), id)["status"] == "failed");
    CHECK(read_capture(root.path(), id, 2)["status"] == "finished");
}

TEST_CASE("InferenceAudit distinguishes response limits from complete input delivery", "[inference-audit]")
{
    PrivateAuditRoot root;
    EnvVarGuard guard(kAuditEnv, root.path().string());
    std::string id;
    auto audit = create_audit(id);
    audit->record_fit("original evidence", "system original evidence", kTokens, "system original evidence");
    audit->submit_prompt(0, kTokens);
    audit->prompt_result(0);
    CHECK_THROWS_AS(audit->prompt_result(0), InferenceAudit::Error);
    audit->record_generation("partial", "generation-token-limit", 8);
    audit->finish("partial");
    const auto capture = read_capture(root.path(), id);
    CHECK(capture["allPromptTokensAcceptedExactly"].toBool());
    CHECK(capture["generation"].toObject()["stopReason"] == "generation-token-limit");
}

TEST_CASE("InferenceAudit fails clearly on invalid output locations", "[inference-audit]")
{
    std::string id;
    SECTION("relative paths are rejected") {
        EnvVarGuard guard(kAuditEnv, "relative-audit-location");
        CHECK_THROWS_AS(create_audit(id), InferenceAudit::Error);
    }
#ifdef _WIN32
    SECTION("invalid UTF-8 paths are audit errors, not backend errors") {
        TempDir root;
        EnvVarGuard guard(kAuditEnv, root.path().string() + "/bad-\xff");
        CHECK_THROWS_AS(create_audit(id), InferenceAudit::Error);
    }
#endif
    SECTION("a regular file is not an output directory") {
        TempDir root;
        const auto target = root.path() / "regular-file";
        QFile file(QString::fromStdString(target.string()));
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.close();
        EnvVarGuard guard(kAuditEnv, target.string());
        CHECK_THROWS_AS(create_audit(id), InferenceAudit::Error);
    }
    SECTION("a new root is created privately") {
        TempDir parent;
        const auto target = parent.path() / "audit";
        EnvVarGuard guard(kAuditEnv, target.string());
        auto audit = create_audit(id);
        REQUIRE(audit);
#ifndef _WIN32
        CHECK((std::filesystem::status(target).permissions() & std::filesystem::perms::all) ==
              std::filesystem::perms::owner_all);
#endif
    }
#ifndef _WIN32
    SECTION("a root symlink is rejected") {
        TempDir parent;
        const auto link = parent.path() / "audit-link";
        std::filesystem::create_directory_symlink(parent.path(), link);
        EnvVarGuard guard(kAuditEnv, link.string());
        CHECK_THROWS_AS(create_audit(id), InferenceAudit::Error);
    }
    SECTION("a shared-writable root is rejected") {
        TempDir root;
        std::filesystem::permissions(root.path(), std::filesystem::perms::others_write,
                                     std::filesystem::perm_options::add);
        EnvVarGuard guard(kAuditEnv, root.path().string());
        CHECK_THROWS_AS(create_audit(id), InferenceAudit::Error);
    }
#endif
}
