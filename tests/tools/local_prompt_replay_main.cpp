#include "AnalysisRuntimeLock.hpp"
#include "LocalLLMClient.hpp"
#include "LocalLLMPromptBuilder.hpp"
#include "FolderTreeLLMRouter.hpp"
#include "FolderTreeSelectionContract.hpp"
#include "FolderStructurePluginProfile.hpp"
#include "NativeJsonSchemaGrammar.hpp"
#include "Utils.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
QString required_string(const QJsonObject& object, const char* key)
{
    const auto value = object.value(key);
    if (!value.isString() || value.toString().isEmpty()) {
        throw std::runtime_error(std::string("Missing nonempty string: ") + key);
    }
    return value.toString();
}

QString digest(const QString& value)
{
    return QString::fromLatin1(QCryptographicHash::hash(value.toUtf8(), QCryptographicHash::Sha256).toHex());
}

using Profiles = std::vector<FolderStructurePluginProfile>;

std::optional<FolderTreeCatalog::Selection> route_production(
    const QJsonObject& input, const Profiles& profiles, const FolderTreeLLMRouter::Request& request)
{
    std::vector<FolderTreeCatalog::Entry> entries;
    for (const auto& path : input.value("folders").toArray()) {
        if (!path.isString()) throw std::runtime_error("Production folder paths must be strings");
        const auto value = path.toString().toStdString();
        const auto valid = FolderTreeCatalog::validate_relative_folder_path(value);
        if (!valid.valid || valid.normalized_path != value) throw std::runtime_error("Invalid production folder path");
        entries.push_back({value, static_cast<int>(std::count(value.begin(), value.end(), '/')) + 1});
    }
    if (entries.empty()) throw std::runtime_error("Production replay requires a nonempty catalog");
    return FolderTreeLLMRouter::select(FolderTreeCatalog::Catalog(std::move(entries)), false,
        input.value("category").toString().toStdString(), input.value("subcategory").toString().toStdString(),
        profiles, request);
}

std::string production_context(const QJsonObject& input, const std::string& routing_context)
{
    return LocalLLMPromptBuilder::build_request_context(input.value("evidence").toString().toStdString(), routing_context);
}

QJsonObject render_production(QJsonObject request, const Profiles& profiles)
{
    const auto input = request.value("productionInput").toObject();
    const auto name = required_string(input, "filename").toStdString();
    int calls = 0;
    route_production(input, profiles, [&](const auto& context, const auto& constraint) {
        if (++calls != 1) throw std::runtime_error("Production replay requires one catalog comparison per case");
        const auto combined = production_context(input, context);
        const auto system = LocalLLMPromptBuilder::build_system_prompt(name, FileType::File, combined);
        const auto user = LocalLLMPromptBuilder::build_user_prompt(name, name, FileType::File, combined);
        for (const auto& [key, value] : std::initializer_list<std::pair<const char*, std::string>>{
                 {"systemPrompt", system}, {"userPrompt", user}, {"jsonSchema", constraint.json_schema}}) {
            if (request.contains(key) && request.value(key).toString().toStdString() != value) {
                throw std::runtime_error(std::string("Production prompt changed since rendering: ") + key);
            }
            request.insert(key, QString::fromStdString(value));
        }
        request.insert("maxTokens", 128);
        return std::string(R"({"targetFolder":null,"createFolder":false})");
    });
    if (calls != 1) throw std::runtime_error("Production rendering did not compare the catalog");
    return request;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        const auto arguments = app.arguments();
        if (arguments.size() < 2 || arguments.size() > 3
            || (arguments.size() == 3 && arguments[2] != "--validate-only" && arguments[2] != "--render-production")) {
            std::cerr << "Usage: local-prompt-replay CONFIG.json [--validate-only|--render-production]\n";
            return 2;
        }
        QFile config_file(arguments[1]);
        if (!config_file.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read replay config");
        QJsonParseError parse_error;
        const auto document = QJsonDocument::fromJson(config_file.readAll(), &parse_error);
        if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
            throw std::runtime_error("Replay config must be a JSON object");
        }
        auto config = document.object();
        auto requests = config.value("requests").toArray();
        if (requests.isEmpty()) throw std::runtime_error("No replay requests");
        Profiles profiles;
        for (const auto& path : config.value("productionProfileFiles").toArray()) {
            std::string error;
            const auto profile = load_folder_structure_plugin_profile_from_file(
                Utils::utf8_to_path(path.toString().toStdString()), {}, &error);
            if (!profile) throw std::runtime_error("Cannot load frozen production profile: " + error);
            profiles.push_back(*profile);
        }
        for (qsizetype i = 0; i < requests.size(); ++i) {
            if (requests[i].toObject().contains("productionInput")) {
                requests[i] = render_production(requests[i].toObject(), profiles);
            }
        }
        config.insert("requests", requests);
        QSet<QString> ids;
        for (const auto& value : requests) {
            const auto request = value.toObject();
            const auto id = required_string(request, "id");
            if (ids.contains(id)) throw std::runtime_error("Duplicate request id");
            ids.insert(id);
            required_string(request, "systemPrompt");
            required_string(request, "userPrompt");
            NativeJsonSchemaGrammar::compile(required_string(request, "jsonSchema").toStdString());
            if (request.value("maxTokens").toInt() <= 0) throw std::runtime_error("Invalid maxTokens");
        }
        if (arguments.size() == 3) {
            if (arguments[2] == "--render-production") {
                const auto path = QFileInfo(arguments[1]).absolutePath() + "/rendered-requests.json";
                QFile output(path);
                if (!output.open(QIODevice::WriteOnly | QIODevice::NewOnly)) throw std::runtime_error("Rendered config already exists");
                const auto bytes = QJsonDocument(config).toJson(QJsonDocument::Indented);
                if (output.write(bytes) != bytes.size()) throw std::runtime_error("Cannot persist rendered config");
                std::cout << path.toStdString() << '\n';
            }
            std::cout << "Validated " << requests.size() << " requests and native response grammars.\n";
            return 0;
        }

        const auto backend = required_string(config, "backend");
        if (backend != "cpu" && backend != "metal" && backend != "cuda" && backend != "vulkan") {
            throw std::runtime_error("An explicit cpu/metal/cuda/vulkan backend is required");
        }
        const auto sampling = config.value("sampling").toObject();
        LocalInferenceOptions inference;
        inference.temperature = static_cast<float>(sampling.value("temperature").toDouble(inference.temperature));
        // Explicit replay sampling keeps controlling the whole experiment; a routing-only value may override it.
        if (sampling.contains("existingFolderTemperature")) {
            inference.existing_folder_temperature = static_cast<float>(sampling.value("existingFolderTemperature").toDouble());
        } else if (sampling.contains("temperature")) {
            inference.existing_folder_temperature = inference.temperature;
        }
        inference.seed = static_cast<std::uint32_t>(sampling.value("seed").toDouble(42));
        inference.min_p = static_cast<float>(sampling.value("minP").toDouble(0.05));
        inference.threads = sampling.value("threads").toInt(4);
        inference.batch_tokens = sampling.value("batchTokens").toInt(2048);
        inference.validate();
        const int context_tokens = config.value("contextTokens").toInt(2048);
        const int timeout_seconds = config.value("timeoutSeconds").toInt(60);
        if (context_tokens <= 0 || timeout_seconds <= 0) throw std::runtime_error("Invalid context/deadline");
        const auto audit_root = Utils::utf8_to_path(required_string(config, "auditDirectory").toStdString());

        AnalysisRuntimeLock lock(Utils::utf8_to_path(required_string(config, "runtimeDirectory").toStdString()));
        AnalysisRuntimeLock::Metadata metadata;
        metadata.owner = AnalysisRuntimeLock::Owner::Headless;
        metadata.description = "Frozen routing prompt replay";
        std::string lock_error;
        auto lease = lock.try_acquire(std::move(metadata), &lock_error);
        if (!lease) throw std::runtime_error("Analysis runtime is busy: " + lock_error);

        QFile output(required_string(config, "outputFile"));
        if (!output.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
            throw std::runtime_error("Results file already exists or cannot be created");
        }
        qputenv("AI_FILE_SORTER_GPU_BACKEND", backend.toUtf8());
        qputenv("LLAMA_ARG_DEVICE", backend.toUtf8());
        qputenv("AI_FILE_SORTER_CTX_TOKENS", QByteArray::number(context_tokens));
#ifdef AIFS_REPLAY_STATIC_GGML
        // The app objects already contain GGML; loading another core can abort at initialization.
        qunsetenv("AI_FILE_SORTER_GGML_DIR");
#endif
        LocalLLMClient::Options options;
        options.required_backend = backend.toStdString();
        options.force_cpu_backend = backend == "cpu";
        if (config.contains("sampling")) options.inference = inference;
        LocalLLMClient client(required_string(config, "modelPath").toStdString(),
            [](const std::string&) { return false; }, options);
        int failures = 0;
        int index = 0;
        for (const auto& value : requests) {
            const auto request = value.toObject();
            const auto id = request.value("id").toString();
            // A separate directory binds every native audit to its exact replay request.
            const auto audit_directory = audit_root / std::to_string(index);
            qputenv("AI_FILE_SORTER_INFERENCE_AUDIT_DIR",
                    QByteArray::fromStdString(Utils::path_to_utf8(audit_directory)));
            QJsonObject result{{"id", id}, {"caseId", request.value("caseId")},
                {"variantId", request.value("variantId")}, {"repeat", request.value("repeat").toBool()},
                {"systemSha256", digest(request.value("systemPrompt").toString())},
                {"userSha256", digest(request.value("userPrompt").toString())},
                {"schemaSha256", digest(request.value("jsonSchema").toString())},
                {"auditDirectory", QString::fromStdString(Utils::path_to_utf8(audit_directory))},
                {"startedAtUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
            const auto start = std::chrono::steady_clock::now();
            client.set_request_deadline(start + std::chrono::seconds(timeout_seconds));
            try {
                std::string response;
                if (request.contains("productionInput")) {
                    const auto input = request.value("productionInput").toObject();
                    const auto name = required_string(input, "filename").toStdString();
                    int calls = 0;
                    const auto selection = route_production(input, profiles, [&](const auto& context, const auto& constraint) {
                        if (++calls != 1) throw std::runtime_error("Production routing needed additional comparisons; inspect native audit");
                        response = client.categorize_file_constrained(name, name, FileType::File,
                            production_context(input, context), constraint);
                        return response;
                    });
                    result.insert("productionRoutingCalls", calls);
                    result.insert("productionTarget", selection ? QJsonValue(QString::fromStdString(selection->relative_path)) : QJsonValue(QJsonValue::Null));
                } else {
                    response = client.generate_response(request.value("userPrompt").toString().toStdString(),
                        request.value("maxTokens").toInt(), false,
                        request.value("systemPrompt").toString().toStdString(),
                        {request.value("jsonSchema").toString().toStdString()});
                }
                result.insert("response", QString::fromStdString(response));
                if (response.empty()) throw std::runtime_error("Empty response or deadline expired");
            } catch (const std::exception& error) {
                result.insert("error", QString::fromUtf8(error.what()));
                ++failures;
            }
            client.set_request_deadline(std::nullopt);
            result.insert("elapsedSeconds", std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
            const auto line = QJsonDocument(result).toJson(QJsonDocument::Compact) + '\n';
            if (output.write(line) != line.size() || !output.flush()) throw std::runtime_error("Cannot persist replay result");
            std::cout << "[replay] " << ++index << '/' << requests.size() << ' '
                      << request.value("variantId").toString().toStdString() << ' '
                      << request.value("caseId").toString().toStdString() << std::endl;
        }
        return failures ? 1 : 0;
    } catch (const std::exception& error) {
        std::cerr << "Prompt replay: " << error.what() << '\n';
        return 1;
    }
}
