#include "InferenceAudit.hpp"
#include "Utils.hpp"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUuid>
#include <algorithm>
#include <cstdlib>

namespace {
constexpr char kAuditDirectoryEnv[] = "AI_FILE_SORTER_INFERENCE_AUDIT_DIR";
constexpr auto kPrivateDirectoryPermissions = std::filesystem::perms::owner_all;
constexpr auto kPrivateFilePermissions = QFileDevice::ReadOwner | QFileDevice::WriteOwner;

QString string_value(std::string_view text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

QJsonObject bytes_value(std::string_view text)
{
    const QByteArray bytes(text.data(), static_cast<qsizetype>(text.size()));
    return {{"text", QString::fromUtf8(bytes)}, {"base64", QString::fromLatin1(bytes.toBase64())},
            {"bytes", static_cast<qint64>(bytes.size())},
            {"sha256", QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex())}};
}

QJsonArray token_values(std::span<const std::int32_t> tokens)
{
    QJsonArray values;
    for (const auto token : tokens) values.append(token);
    return values;
}

void make_private_directory(const std::filesystem::path& directory)
{
    std::error_code error;
    const bool created = std::filesystem::create_directory(directory, error);
    if (!created || error) throw InferenceAudit::Error("Unable to create a unique inference audit directory");
#ifndef _WIN32
    std::filesystem::permissions(directory, kPrivateDirectoryPermissions,
                                 std::filesystem::perm_options::replace, error);
    if (error) throw InferenceAudit::Error("Unable to restrict inference audit directory permissions");
#endif
}

void prepare_audit_root(const std::filesystem::path& root)
{
    if (!root.is_absolute()) throw InferenceAudit::Error("Inference audit directory must be an absolute path");
    std::error_code error;
    if (std::filesystem::is_symlink(root, error)) throw InferenceAudit::Error("Inference audit root must not be a symbolic link");
    [[maybe_unused]] const bool created = std::filesystem::create_directories(root, error);
    if (error) throw InferenceAudit::Error("Unable to create inference audit root");
#ifndef _WIN32
    if (created) std::filesystem::permissions(root, kPrivateDirectoryPermissions,
        std::filesystem::perm_options::replace, error);
    if (error) throw InferenceAudit::Error("Unable to restrict inference audit root permissions");
    const auto public_write = std::filesystem::perms::group_write | std::filesystem::perms::others_write;
    const auto permissions = std::filesystem::status(root, error).permissions();
    if (error || (permissions & public_write) != std::filesystem::perms::none) {
        throw InferenceAudit::Error("Inference audit root must not be writable by other users");
    }
#endif
}

std::filesystem::path configured_audit_path(const char* configured)
{
    try {
        return Utils::utf8_to_path(configured);
    } catch (const std::exception&) {
        throw InferenceAudit::Error("Unable to decode the inference audit directory path");
    }
}
} // namespace

InferenceAudit::InferenceAudit(const std::filesystem::path& directory,
                               const std::string& request_id, int attempt)
{
    file_ = directory / ("attempt-" + std::to_string(attempt) + ".json");
    record_ = {{"schemaVersion", 1}, {"requestId", string_value(request_id)}, {"attempt", attempt},
               {"createdAtUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
               {"status", "started"}, {"boundary", "application calls to llama_decode; not GPU kernel internals"}};
}

std::unique_ptr<InferenceAudit> InferenceAudit::from_environment(
    const std::string& model_path, std::string_view system_prompt, std::string_view user_prompt,
    std::string_view chat_template, std::string& request_id, int attempt)
{
    const char* configured = std::getenv(kAuditDirectoryEnv);
    if (!configured || !*configured) return {};
    const auto root = configured_audit_path(configured);
    prepare_audit_root(root);
    if (request_id.empty()) {
        request_id = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
        make_private_directory(root / request_id);
    }
    auto audit = std::unique_ptr<InferenceAudit>(new InferenceAudit(root / request_id, request_id, attempt));
    audit->original_user_ = user_prompt;
    audit->record_.insert("modelPath", string_value(model_path));
    audit->record_.insert("systemPrompt", bytes_value(system_prompt));
    audit->record_.insert("originalUserPrompt", bytes_value(user_prompt));
    audit->record_.insert("chatTemplate", bytes_value(chat_template));
    audit->record_.insert("templateMode", chat_template.empty() ? "plain-text-fallback" : "llama_chat_apply_template");
    audit->persist();
    return audit;
}

InferenceAudit::~InferenceAudit()
{
    if (finalized_) return;
    try {
        record_.insert("status", "incomplete");
        update_verdict();
        persist();
    } catch (...) {
        // Destruction cannot throw. The last atomic checkpoint remains explicitly unfinished.
    }
}

void InferenceAudit::persist() const
{
    QSaveFile file(QString::fromStdString(Utils::path_to_utf8(file_)));
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(kPrivateFilePermissions)) {
        throw Error("Unable to open an owner-only inference audit file");
    }
    auto record = record_;
    record.insert("fitAttempts", fits_);
    record.insert("promptDecodeCalls", chunks_);
    const auto bytes = QJsonDocument(record).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size() || !file.commit()) throw Error("Unable to commit inference audit capture");
}

void InferenceAudit::set_runtime(const Runtime& runtime)
{
    record_.insert("runtime", QJsonObject{{"contextTokens", runtime.context_tokens},
        {"promptBudget", runtime.prompt_budget}, {"generationLimit", runtime.generation_limit},
        {"batchTokens", runtime.batch_tokens}, {"seed", static_cast<qint64>(runtime.seed)},
        {"generationThreads", runtime.generation_threads}, {"batchThreads", runtime.batch_threads},
        {"temperature", runtime.temperature}, {"minP", runtime.min_p}, {"requestedBackend", string_value(runtime.backend)}});
    persist();
}

void InferenceAudit::record_fit(std::string_view user_prompt, std::string_view formatted_prompt,
    std::span<const std::int32_t> tokens, std::string_view detokenized)
{
    if (!chunks_.empty()) throw Error("Cannot fit another prompt after prompt decoding has started");
    const int budget = record_.value("runtime").toObject().value("promptBudget").toInt();
    fits_.append(QJsonObject{{"userPrompt", bytes_value(user_prompt)}, {"formattedPrompt", bytes_value(formatted_prompt)},
        {"tokenIds", token_values(tokens)}, {"detokenizedPrompt", bytes_value(detokenized)},
        {"tokenCount", static_cast<qint64>(tokens.size())}, {"withinBudget", tokens.size() <= static_cast<std::size_t>(budget)},
        {"userPromptChanged", user_prompt != original_user_},
        {"formattedTextPresentInTokens", detokenized.find(formatted_prompt) != std::string_view::npos}});
    expected_.assign(tokens.begin(), tokens.end());
    persist();
}

void InferenceAudit::submit_prompt(std::size_t offset, std::span<const std::int32_t> tokens)
{
    if (!chunks_.empty() && chunks_.last().toObject().value("status") == "submitted") {
        throw Error("Cannot submit another prompt chunk before recording the previous decode result");
    }
    const bool within = offset <= expected_.size() && tokens.size() <= expected_.size() - offset;
    const bool matches = within && std::equal(tokens.begin(), tokens.end(), expected_.begin() + offset);
    all_chunks_match_ = all_chunks_match_ && matches && offset == decoded_count_;
    record_.insert("selectedFitAttempt", fits_.size() - 1);
    chunks_.append(QJsonObject{{"offset", static_cast<qint64>(offset)}, {"tokenIds", token_values(tokens)},
        {"matchesFittedTokens", matches}, {"status", "submitted"}});
    update_verdict();
    persist();
}

void InferenceAudit::prompt_result(int status)
{
    if (chunks_.empty() || chunks_.last().toObject().value("status") != "submitted") {
        throw Error("Prompt decode result has no pending audit submission");
    }
    auto chunk = chunks_.last().toObject();
    chunk.insert("decodeStatus", status);
    chunk.insert("status", status == 0 ? "accepted" : "failed");
    if (status == 0) decoded_count_ += static_cast<std::size_t>(chunk.value("tokenIds").toArray().size());
    else all_chunks_match_ = false;
    chunks_.replace(chunks_.size() - 1, chunk);
    update_verdict();
    persist();
}

void InferenceAudit::update_verdict()
{
    record_.insert("acceptedPromptTokens", static_cast<qint64>(decoded_count_));
    const bool accepted = !chunks_.empty() && chunks_.last().toObject().value("status") == "accepted";
    record_.insert("allPromptTokensAcceptedExactly", accepted && !expected_.empty() &&
                   all_chunks_match_ && decoded_count_ == expected_.size());
}

void InferenceAudit::record_generation(std::string_view raw_response, std::string_view stop_reason, int generated_tokens)
{
    record_.insert("generation", QJsonObject{{"rawResponse", bytes_value(raw_response)},
        {"stopReason", string_value(stop_reason)}, {"generatedTokens", generated_tokens}});
    persist();
}

void InferenceAudit::finish(std::string_view response)
{
    record_.insert("returnedResponse", bytes_value(response));
    record_.insert("status", "finished");
    update_verdict();
    persist();
    finalized_ = true;
}

void InferenceAudit::fail(std::string_view message)
{
    record_.insert("error", string_value(message));
    record_.insert("status", "failed");
    update_verdict();
    persist();
    finalized_ = true;
}
