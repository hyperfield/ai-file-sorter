#include "HeadlessAnalysisCommand.hpp"

#include "AnalysisRunResult.hpp"
#include "DatabaseManager.hpp"
#include "HeadlessAnalysisWorkflowHost.hpp"
#include "HeadlessReviewApplyService.hpp"
#include "LocalFsProvider.hpp"
#include "Logger.hpp"
#include "ReviewHistoryStore.hpp"
#include "Settings.hpp"
#include "Utils.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QString>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <system_error>
#include <utility>

namespace {

constexpr int kStatusSchemaVersion = 1;
constexpr int kReviewPlanSchemaVersion = 1;
constexpr char kReviewPlanKind[] = "aifs.headlessReviewPlan";

QString to_qstring(const std::filesystem::path& path)
{
#ifdef _WIN32
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}

std::string utc_now_iso()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();
}

std::string lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string generate_job_id()
{
    return "headless-" + std::to_string(QCoreApplication::applicationPid()) + "-" +
           std::to_string(QDateTime::currentMSecsSinceEpoch());
}

bool is_headless_request_flag(const std::string& argument)
{
    return argument == "--headless" || argument == "--headless-apply" ||
           argument == "--headless-help";
}

bool argument_has_prefix(const std::string& argument, const std::string& prefix)
{
    return argument.rfind(prefix, 0) == 0;
}

std::optional<std::string> value_from_inline_argument(const std::string& argument,
                                                      const std::string& prefix)
{
    if (!argument_has_prefix(argument, prefix + "=")) {
        return std::nullopt;
    }
    return argument.substr(prefix.size() + 1);
}

bool is_value_argument(const std::string& argument)
{
    return argument == "--operation" || argument == "--headless-operation" ||
           argument == "--path" || argument == "--headless-path" ||
           argument == "--status-file" || argument == "--headless-status-file" ||
           argument == "--job-id" || argument == "--headless-job-id" ||
           argument == "--review-file" || argument == "--headless-review-file" ||
           argument == "--settings-overrides-file" ||
           argument == "--headless-settings-overrides-file";
}

bool parse_value(int argc,
                 char** argv,
                 int& index,
                 const std::string& argument,
                 const std::string& long_name,
                 const std::string& headless_name,
                 std::string* value,
                 HeadlessAnalysisCommand::ParseResult* result)
{
    if (const auto inline_value = value_from_inline_argument(argument, long_name)) {
        *value = *inline_value;
        result->consumed_arguments[static_cast<std::size_t>(index)] = true;
        return true;
    }
    if (const auto inline_value = value_from_inline_argument(argument, headless_name)) {
        *value = *inline_value;
        result->consumed_arguments[static_cast<std::size_t>(index)] = true;
        return true;
    }
    if (argument != long_name && argument != headless_name) {
        return false;
    }

    result->consumed_arguments[static_cast<std::size_t>(index)] = true;
    if (index + 1 >= argc || !argv[index + 1]) {
        result->error = "Missing value for " + argument + ".";
        return true;
    }
    ++index;
    *value = argv[index];
    result->consumed_arguments[static_cast<std::size_t>(index)] = true;
    return true;
}

QJsonObject metadata_to_json(const AnalysisRuntimeLock::Metadata& metadata)
{
    QJsonObject object;
    object.insert(QStringLiteral("owner"),
                  QString::fromStdString(AnalysisRuntimeLock::owner_to_string(metadata.owner)));
    object.insert(QStringLiteral("pid"), QString::number(metadata.pid));
    object.insert(QStringLiteral("jobId"), QString::fromStdString(metadata.job_id));
    object.insert(QStringLiteral("startedAtUtc"), QString::fromStdString(metadata.started_at_utc));
    object.insert(QStringLiteral("description"), QString::fromStdString(metadata.description));
    return object;
}

QJsonObject status_to_json(const HeadlessAnalysisCommand::Options& options,
                           const std::string& status,
                           const std::string& message,
                           const std::string& error,
                           const std::optional<AnalysisRuntimeLock::Metadata>& lock_metadata,
                           const QJsonObject* extra_payload = nullptr)
{
    QJsonObject object;
    object.insert(QStringLiteral("schemaVersion"), kStatusSchemaVersion);
    object.insert(QStringLiteral("status"), QString::fromStdString(status));
    object.insert(QStringLiteral("operation"),
                  QString::fromStdString(HeadlessAnalysisCommand::operation_to_string(options.operation)));
    object.insert(QStringLiteral("jobId"), QString::fromStdString(options.job_id));
    object.insert(QStringLiteral("message"), QString::fromStdString(message));
    object.insert(QStringLiteral("error"), QString::fromStdString(error));
    object.insert(QStringLiteral("updatedAtUtc"), QString::fromStdString(utc_now_iso()));

    QJsonArray paths;
    for (const auto& path : options.paths) {
        paths.append(QString::fromStdString(Utils::path_to_utf8(path)));
    }
    object.insert(QStringLiteral("paths"), paths);

    if (lock_metadata) {
        object.insert(QStringLiteral("lock"), metadata_to_json(*lock_metadata));
    }
    if (extra_payload) {
        for (auto it = extra_payload->begin(); it != extra_payload->end(); ++it) {
            object.insert(it.key(), it.value());
        }
    }
    return object;
}

bool write_status_file(const std::filesystem::path& path,
                       const QJsonDocument& document,
                       std::string* error)
{
    std::error_code ec;
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            if (error) {
                *error = ec.message();
            }
            return false;
        }
    }

    QSaveFile file(to_qstring(path));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = file.errorString().toStdString();
        }
        return false;
    }

    const QByteArray payload = document.toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size()) {
        if (error) {
            *error = file.errorString().toStdString();
        }
        return false;
    }
    if (!file.commit()) {
        if (error) {
            *error = file.errorString().toStdString();
        }
        return false;
    }
    return true;
}

bool emit_status(const HeadlessAnalysisCommand::Options& options,
                 const std::string& status,
                 const std::string& message,
                 const std::string& error,
                 const std::optional<AnalysisRuntimeLock::Metadata>& lock_metadata,
                 std::ostream& out,
                 std::ostream& err,
                 const QJsonObject* extra_payload = nullptr)
{
    const QJsonDocument document(
        status_to_json(options, status, message, error, lock_metadata, extra_payload));
    out << document.toJson(QJsonDocument::Compact).constData() << '\n';

    if (!options.status_file) {
        return true;
    }

    std::string file_error;
    if (!write_status_file(*options.status_file, document, &file_error)) {
        err << "Could not write headless status file: " << file_error << '\n';
        return false;
    }
    return true;
}

QJsonObject apply_result_to_json(const HeadlessReviewApplyService::Result& result)
{
    QJsonArray entries;
    for (const auto& entry : result.entries) {
        QJsonObject object;
        object.insert(QStringLiteral("source"), QString::fromStdString(entry.source));
        object.insert(QStringLiteral("destination"), QString::fromStdString(entry.destination));
        object.insert(QStringLiteral("fileName"), QString::fromStdString(entry.file_name));
        object.insert(QStringLiteral("destinationName"), QString::fromStdString(entry.destination_name));
        object.insert(QStringLiteral("category"), QString::fromStdString(entry.category));
        object.insert(QStringLiteral("subcategory"), QString::fromStdString(entry.subcategory));
        object.insert(QStringLiteral("message"), QString::fromStdString(entry.message));
        object.insert(QStringLiteral("renameOnly"), entry.rename_only);
        object.insert(QStringLiteral("moved"), entry.moved);
        object.insert(QStringLiteral("renamed"), entry.renamed);
        object.insert(QStringLiteral("skipped"), entry.skipped);
        entries.append(object);
    }

    QJsonObject review;
    review.insert(QStringLiteral("entryCount"), static_cast<qint64>(result.planned_count));
    review.insert(QStringLiteral("entries"), entries);

    QJsonObject apply;
    apply.insert(QStringLiteral("movedCount"), static_cast<qint64>(result.moved_count));
    apply.insert(QStringLiteral("renamedCount"), static_cast<qint64>(result.renamed_count));
    apply.insert(QStringLiteral("skippedCount"), static_cast<qint64>(result.skipped_count));
    apply.insert(QStringLiteral("undoPlanSaved"), result.undo_plan_saved);

    QJsonObject payload;
    payload.insert(QStringLiteral("review"), review);
    payload.insert(QStringLiteral("apply"), apply);
    return payload;
}

QJsonObject apply_result_to_json(const HeadlessReviewApplyService::Result& result,
                                 bool review_required)
{
    QJsonObject payload = apply_result_to_json(result);
    payload.insert(QStringLiteral("reviewRequired"), review_required);
    QJsonObject review = payload.value(QStringLiteral("review")).toObject();
    review.insert(QStringLiteral("requiresApproval"), review_required);
    payload.insert(QStringLiteral("review"), review);
    return payload;
}

QString file_type_to_json(FileType type)
{
    return type == FileType::Directory ? QStringLiteral("directory") : QStringLiteral("file");
}

FileType file_type_from_json(const QString& value)
{
    return value.compare(QStringLiteral("directory"), Qt::CaseInsensitive) == 0
        ? FileType::Directory
        : FileType::File;
}

QJsonObject categorized_file_to_json(const CategorizedFile& entry)
{
    QJsonObject object;
    object.insert(QStringLiteral("filePath"), QString::fromStdString(entry.file_path));
    object.insert(QStringLiteral("fileName"), QString::fromStdString(entry.file_name));
    object.insert(QStringLiteral("type"), file_type_to_json(entry.type));
    object.insert(QStringLiteral("category"), QString::fromStdString(entry.category));
    object.insert(QStringLiteral("subcategory"), QString::fromStdString(entry.subcategory));
    object.insert(QStringLiteral("taxonomyId"), entry.taxonomy_id);
    object.insert(QStringLiteral("fromCache"), entry.from_cache);
    object.insert(QStringLiteral("usedConsistencyHints"), entry.used_consistency_hints);
    object.insert(QStringLiteral("suggestedName"), QString::fromStdString(entry.suggested_name));
    object.insert(QStringLiteral("renameOnly"), entry.rename_only);
    object.insert(QStringLiteral("renameApplied"), entry.rename_applied);
    object.insert(QStringLiteral("canonicalCategory"), QString::fromStdString(entry.canonical_category));
    object.insert(QStringLiteral("canonicalSubcategory"), QString::fromStdString(entry.canonical_subcategory));
    object.insert(QStringLiteral("learningContext"), QString::fromStdString(entry.learning_context));
    return object;
}

CategorizedFile categorized_file_from_json(const QJsonObject& object)
{
    CategorizedFile entry;
    entry.file_path = object.value(QStringLiteral("filePath")).toString().toStdString();
    entry.file_name = object.value(QStringLiteral("fileName")).toString().toStdString();
    entry.type = file_type_from_json(object.value(QStringLiteral("type")).toString());
    entry.category = object.value(QStringLiteral("category")).toString().toStdString();
    entry.subcategory = object.value(QStringLiteral("subcategory")).toString().toStdString();
    entry.taxonomy_id = object.value(QStringLiteral("taxonomyId")).toInt();
    entry.from_cache = object.value(QStringLiteral("fromCache")).toBool();
    entry.used_consistency_hints = object.value(QStringLiteral("usedConsistencyHints")).toBool();
    entry.suggested_name = object.value(QStringLiteral("suggestedName")).toString().toStdString();
    entry.rename_only = object.value(QStringLiteral("renameOnly")).toBool();
    entry.rename_applied = object.value(QStringLiteral("renameApplied")).toBool();
    entry.canonical_category = object.value(QStringLiteral("canonicalCategory")).toString().toStdString();
    entry.canonical_subcategory = object.value(QStringLiteral("canonicalSubcategory")).toString().toStdString();
    entry.learning_context = object.value(QStringLiteral("learningContext")).toString().toStdString();
    return entry;
}

QJsonObject apply_options_to_json(const HeadlessReviewApplyService::Options& options)
{
    QJsonObject object;
    object.insert(QStringLiteral("baseDir"), QString::fromStdString(options.base_dir));
    object.insert(QStringLiteral("undoDir"), QString::fromStdString(options.undo_dir));
    object.insert(QStringLiteral("useSubcategories"), options.use_subcategories);
    object.insert(QStringLiteral("includeSubdirectories"), options.include_subdirectories);
    object.insert(QStringLiteral("applySuggestedNames"), options.apply_suggested_names);
    object.insert(QStringLiteral("moveCategorizedEntries"), options.move_categorized_entries);
    object.insert(QStringLiteral("categoryLanguage"), categoryLanguageToString(options.category_language));
    return object;
}

HeadlessReviewApplyService::Options apply_options_from_json(const QJsonObject& object)
{
    HeadlessReviewApplyService::Options options;
    options.base_dir = object.value(QStringLiteral("baseDir")).toString().toStdString();
    options.undo_dir = object.value(QStringLiteral("undoDir")).toString().toStdString();
    options.use_subcategories = object.value(QStringLiteral("useSubcategories")).toBool(true);
    options.include_subdirectories = object.value(QStringLiteral("includeSubdirectories")).toBool(false);
    options.apply_suggested_names = object.value(QStringLiteral("applySuggestedNames")).toBool(false);
    options.move_categorized_entries = object.value(QStringLiteral("moveCategorizedEntries")).toBool(true);
    options.category_language =
        categoryLanguageFromString(object.value(QStringLiteral("categoryLanguage")).toString());
    options.apply_changes = true;
    return options;
}

std::optional<bool> optional_bool_from_json(const QJsonObject& object, QStringView key)
{
    const QJsonValue value = object.value(key);
    if (!value.isBool()) {
        return std::nullopt;
    }
    return value.toBool();
}

template <typename Setter>
void set_optional_bool(const QJsonObject& object, QStringView key, Setter setter)
{
    if (const auto value = optional_bool_from_json(object, key)) {
        setter(*value);
    }
}

HeadlessSettingsOverrides settings_overrides_from_json(const QJsonObject& object)
{
    HeadlessSettingsOverrides overrides;
    set_optional_bool(object, QStringLiteral("useSubcategories"), [&](bool value) {
        overrides.use_subcategories = value;
    });
    set_optional_bool(object, QStringLiteral("useConsistencyHints"), [&](bool value) {
        overrides.use_consistency_hints = value;
    });
    set_optional_bool(object, QStringLiteral("useWhitelist"), [&](bool value) {
        overrides.use_whitelist = value;
    });
    const QJsonValue active_whitelist = object.value(QStringLiteral("activeWhitelist"));
    if (active_whitelist.isString()) {
        overrides.active_whitelist = active_whitelist.toString().toStdString();
    }
    set_optional_bool(object, QStringLiteral("categorizeFiles"), [&](bool value) {
        overrides.categorize_files = value;
    });
    set_optional_bool(object, QStringLiteral("categorizeDirectories"), [&](bool value) {
        overrides.categorize_directories = value;
    });
    set_optional_bool(object, QStringLiteral("includeSubdirectories"), [&](bool value) {
        overrides.include_subdirectories = value;
    });
    set_optional_bool(object, QStringLiteral("analyzeImagesByContent"), [&](bool value) {
        overrides.analyze_images_by_content = value;
    });
    set_optional_bool(object, QStringLiteral("processImagesOnly"), [&](bool value) {
        overrides.process_images_only = value;
    });
    set_optional_bool(object, QStringLiteral("addImageDateToCategory"), [&](bool value) {
        overrides.add_image_date_to_category = value;
    });
    set_optional_bool(object, QStringLiteral("addImageDatePlaceToFilename"), [&](bool value) {
        overrides.add_image_date_place_to_filename = value;
    });
    set_optional_bool(object, QStringLiteral("offerRenameImages"), [&](bool value) {
        overrides.offer_rename_images = value;
    });
    set_optional_bool(object, QStringLiteral("renameImagesOnly"), [&](bool value) {
        overrides.rename_images_only = value;
    });
    set_optional_bool(object, QStringLiteral("addAudioVideoMetadataToFilename"), [&](bool value) {
        overrides.add_audio_video_metadata_to_filename = value;
    });
    set_optional_bool(object, QStringLiteral("analyzeDocumentsByContent"), [&](bool value) {
        overrides.analyze_documents_by_content = value;
    });
    set_optional_bool(object, QStringLiteral("processDocumentsOnly"), [&](bool value) {
        overrides.process_documents_only = value;
    });
    set_optional_bool(object, QStringLiteral("offerRenameDocuments"), [&](bool value) {
        overrides.offer_rename_documents = value;
    });
    set_optional_bool(object, QStringLiteral("renameDocumentsOnly"), [&](bool value) {
        overrides.rename_documents_only = value;
    });
    set_optional_bool(object, QStringLiteral("addDocumentDateToCategory"), [&](bool value) {
        overrides.add_document_date_to_category = value;
    });

    const QJsonValue language = object.value(QStringLiteral("language"));
    if (language.isString()) {
        overrides.language = languageFromString(language.toString());
    }
    const QJsonValue category_language = object.value(QStringLiteral("categoryLanguage"));
    if (category_language.isString()) {
        overrides.category_language = categoryLanguageFromString(category_language.toString());
    }
    return overrides;
}

bool read_settings_overrides_file(const std::filesystem::path& path,
                                  HeadlessSettingsOverrides* overrides,
                                  std::string* error)
{
    if (!overrides) {
        return false;
    }
    QFile file(to_qstring(path));
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = "Could not open the headless settings overrides file.";
        }
        return false;
    }

    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) {
            *error = "The headless settings overrides file is not valid JSON.";
        }
        return false;
    }

    *overrides = settings_overrides_from_json(document.object());
    return true;
}

struct HeadlessReviewPlan {
    HeadlessAnalysisCommand::Operation operation{HeadlessAnalysisCommand::Operation::Unknown};
    std::vector<std::filesystem::path> paths;
    HeadlessReviewApplyService::Options apply_options;
    std::vector<CategorizedFile> entries;
};

QJsonObject review_plan_to_json(const HeadlessAnalysisCommand::Options& options,
                                const std::vector<CategorizedFile>& entries,
                                const HeadlessReviewApplyService::Options& apply_options)
{
    QJsonArray paths;
    for (const auto& path : options.paths) {
        paths.append(QString::fromStdString(Utils::path_to_utf8(path)));
    }

    QJsonArray serialized_entries;
    for (const auto& entry : entries) {
        serialized_entries.append(categorized_file_to_json(entry));
    }

    QJsonObject object;
    object.insert(QStringLiteral("schemaVersion"), kReviewPlanSchemaVersion);
    object.insert(QStringLiteral("kind"), QString::fromLatin1(kReviewPlanKind));
    object.insert(QStringLiteral("createdAtUtc"), QString::fromStdString(utc_now_iso()));
    object.insert(QStringLiteral("jobId"), QString::fromStdString(options.job_id));
    object.insert(QStringLiteral("operation"),
                  QString::fromStdString(HeadlessAnalysisCommand::operation_to_string(options.operation)));
    object.insert(QStringLiteral("paths"), paths);
    object.insert(QStringLiteral("applyOptions"), apply_options_to_json(apply_options));
    object.insert(QStringLiteral("entries"), serialized_entries);
    return object;
}

std::optional<std::filesystem::path>
review_file_path_for(const HeadlessAnalysisCommand::Options& options,
                     const std::filesystem::path& runtime_dir)
{
    if (options.review_file) {
        return options.review_file;
    }
    if (options.status_file) {
        const auto& status = *options.status_file;
        return status.parent_path() / (status.stem().wstring() + L".review.json");
    }
    if (!options.job_id.empty()) {
        return runtime_dir / "review-plans" / (options.job_id + ".review.json");
    }
    return std::nullopt;
}

bool write_review_plan_file(const std::filesystem::path& path,
                            const HeadlessAnalysisCommand::Options& options,
                            const std::vector<CategorizedFile>& entries,
                            const HeadlessReviewApplyService::Options& apply_options,
                            std::string* error)
{
    const QJsonDocument document(review_plan_to_json(options, entries, apply_options));
    return write_status_file(path, document, error);
}

bool read_review_plan_file(const std::filesystem::path& path,
                           HeadlessReviewPlan* plan,
                           std::string* error)
{
    if (!plan) {
        if (error) {
            *error = "Review plan output is unavailable.";
        }
        return false;
    }

    QFile file(to_qstring(path));
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = "Could not open review plan: " + file.errorString().toStdString();
        }
        return false;
    }

    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) {
            *error = "Review plan is not valid JSON: " + parse_error.errorString().toStdString();
        }
        return false;
    }

    const QJsonObject object = document.object();
    if (object.value(QStringLiteral("schemaVersion")).toInt() != kReviewPlanSchemaVersion ||
        object.value(QStringLiteral("kind")).toString() != QString::fromLatin1(kReviewPlanKind)) {
        if (error) {
            *error = "Review plan has an unsupported schema.";
        }
        return false;
    }

    HeadlessReviewPlan parsed;
    parsed.operation =
        HeadlessAnalysisCommand::operation_from_string(
            object.value(QStringLiteral("operation")).toString().toStdString());
    if (parsed.operation == HeadlessAnalysisCommand::Operation::Unknown) {
        if (error) {
            *error = "Review plan operation is missing or unsupported.";
        }
        return false;
    }

    for (const auto& value : object.value(QStringLiteral("paths")).toArray()) {
        parsed.paths.push_back(Utils::utf8_to_path(value.toString().toStdString()));
    }
    parsed.apply_options =
        apply_options_from_json(object.value(QStringLiteral("applyOptions")).toObject());
    for (const auto& value : object.value(QStringLiteral("entries")).toArray()) {
        if (value.isObject()) {
            parsed.entries.push_back(categorized_file_from_json(value.toObject()));
        }
    }
    if (parsed.entries.empty()) {
        if (error) {
            *error = "Review plan contains no entries to apply.";
        }
        return false;
    }

    *plan = std::move(parsed);
    return true;
}

bool path_exists(const std::filesystem::path& path)
{
    std::error_code ec;
    return std::filesystem::exists(path, ec) && !ec;
}

bool path_is_supported_target(const std::filesystem::path& path)
{
    std::error_code ec;
    return (std::filesystem::is_regular_file(path, ec) && !ec) ||
           (std::filesystem::is_directory(path, ec) && !ec);
}

std::optional<std::string> validate_analysis_options(const HeadlessAnalysisCommand::Options& options)
{
    if (options.operation == HeadlessAnalysisCommand::Operation::Unknown) {
        return "Missing or invalid headless operation.";
    }
    if (options.paths.empty()) {
        return "At least one --path value is required.";
    }
    for (const auto& path : options.paths) {
        if (!path_exists(path)) {
            return "Path does not exist: " + Utils::path_to_utf8(path);
        }
        if (!path_is_supported_target(path)) {
            return "Path is not a regular file or directory: " + Utils::path_to_utf8(path);
        }
    }
    return std::nullopt;
}

std::optional<std::string> validate_review_apply_options(const HeadlessAnalysisCommand::Options& options)
{
    if (!options.review_file) {
        return "A --review-file value is required for --headless-apply.";
    }
    if (!path_exists(*options.review_file)) {
        return "Review file does not exist: " + Utils::path_to_utf8(*options.review_file);
    }
    std::error_code ec;
    if (!std::filesystem::is_regular_file(*options.review_file, ec) || ec) {
        return "Review file is not a regular file: " + Utils::path_to_utf8(*options.review_file);
    }
    return std::nullopt;
}

std::optional<std::string> validate_options(const HeadlessAnalysisCommand::Options& options)
{
    if (options.request_mode == HeadlessAnalysisCommand::RequestMode::ApplyReview) {
        return validate_review_apply_options(options);
    }
    return validate_analysis_options(options);
}

bool is_directory_target(const std::filesystem::path& path)
{
    std::error_code ec;
    return std::filesystem::is_directory(path, ec) && !ec;
}

bool is_regular_file_target(const std::filesystem::path& path)
{
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec) && !ec;
}

bool operation_applies_suggested_names(HeadlessAnalysisCommand::Operation operation)
{
    return operation == HeadlessAnalysisCommand::Operation::Rename ||
           operation == HeadlessAnalysisCommand::Operation::CategorizeAndRename;
}

bool operation_moves_categorized_entries(HeadlessAnalysisCommand::Operation operation)
{
    return operation == HeadlessAnalysisCommand::Operation::Categorize ||
           operation == HeadlessAnalysisCommand::Operation::CategorizeAndRename;
}

HeadlessAnalysisWorkflowHost::OperationMode
host_operation_mode(HeadlessAnalysisCommand::Operation operation)
{
    switch (operation) {
    case HeadlessAnalysisCommand::Operation::Rename:
        return HeadlessAnalysisWorkflowHost::OperationMode::Rename;
    case HeadlessAnalysisCommand::Operation::CategorizeAndRename:
        return HeadlessAnalysisWorkflowHost::OperationMode::CategorizeAndRename;
    case HeadlessAnalysisCommand::Operation::Categorize:
    case HeadlessAnalysisCommand::Operation::Unknown:
    default:
        return HeadlessAnalysisWorkflowHost::OperationMode::Categorize;
    }
}

bool should_apply_changes(HeadlessAnalysisCommand::ApplyMode mode,
                          const HeadlessAnalysisWorkflowHost& host)
{
    switch (mode) {
    case HeadlessAnalysisCommand::ApplyMode::AutoApply:
        return true;
    case HeadlessAnalysisCommand::ApplyMode::ReviewOnly:
        return false;
    case HeadlessAnalysisCommand::ApplyMode::UseSettings:
    default:
        return !host.headless_review_before_apply();
    }
}

bool has_actionable_review_entries(const HeadlessReviewApplyService::Result& result)
{
    return std::any_of(result.entries.cbegin(), result.entries.cend(), [](const auto& entry) {
        return !entry.skipped;
    });
}

std::filesystem::path normalized_absolute_path(const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::path normalized = std::filesystem::absolute(path, ec);
    if (ec) {
        normalized = path;
    }
    return normalized.lexically_normal();
}

std::string normalized_path_key(const std::filesystem::path& path)
{
    std::string key = Utils::path_to_utf8(normalized_absolute_path(path));
#ifdef _WIN32
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
#endif
    return key;
}

struct ResolvedHeadlessTarget {
    std::filesystem::path folder_path;
    std::vector<std::filesystem::path> selected_paths;
};

std::optional<ResolvedHeadlessTarget>
resolve_headless_target(const HeadlessAnalysisCommand::Options& options,
                        std::string* error)
{
    if (options.paths.size() == 1 && is_directory_target(options.paths.front())) {
        return ResolvedHeadlessTarget{options.paths.front(), {}};
    }

    ResolvedHeadlessTarget target;
    std::string parent_key;
    for (const auto& path : options.paths) {
        if (!is_regular_file_target(path)) {
            if (error) {
                *error = "This build supports either one folder target or same-folder file selections.";
            }
            return std::nullopt;
        }

        const std::filesystem::path selected_path = normalized_absolute_path(path);
        const std::filesystem::path parent_path = selected_path.parent_path();
        const std::string current_parent_key = normalized_path_key(parent_path);
        if (parent_key.empty()) {
            parent_key = current_parent_key;
            target.folder_path = parent_path;
        } else if (current_parent_key != parent_key) {
            if (error) {
                *error = "This build supports same-folder file selections. "
                         "Search-results or cross-folder aggregation will be added in a later plugin slice.";
            }
            return std::nullopt;
        }
        target.selected_paths.push_back(selected_path);
    }

    if (target.folder_path.empty()) {
        if (error) {
            *error = "This build supports one folder target or same-folder file selections.";
        }
        return std::nullopt;
    }
    return target;
}

int finish_analysis_run(const HeadlessAnalysisCommand::Options& options,
                        const AnalysisRunResult& result,
                        const std::optional<HeadlessReviewApplyService::Result>& apply_result,
                        const AnalysisRuntimeLock::Metadata& metadata,
                        std::ostream& out,
                        std::ostream& err)
{
    switch (result.status) {
    case AnalysisRunStatus::Completed: {
        const std::size_t review_entry_count =
            apply_result ? apply_result->planned_count : std::size_t{0};
        std::ostringstream message;
        message << "Headless " << HeadlessAnalysisCommand::operation_to_string(options.operation)
                << " completed. Review entries: " << review_entry_count << ".";
        if (apply_result) {
            message << " Moved: " << apply_result->moved_count
                    << ". Renamed: " << apply_result->renamed_count
                    << ". Skipped: " << apply_result->skipped_count << ".";
            const QJsonObject payload = apply_result_to_json(*apply_result, false);
            emit_status(options, "completed", message.str(), {}, metadata, out, err, &payload);
        } else {
            emit_status(options, "completed", message.str(), {}, metadata, out, err);
        }
        return HeadlessAnalysisCommand::Success;
    }
    case AnalysisRunStatus::Cancelled:
        emit_status(options,
                    "cancelled",
                    "Headless categorization was cancelled.",
                    result.error_message,
                    metadata,
                    out,
                    err);
        return HeadlessAnalysisCommand::Failure;
    case AnalysisRunStatus::Failed:
        emit_status(options,
                    "failed",
                    "Headless categorization failed.",
                    result.error_message,
                    metadata,
                    out,
                    err);
        return HeadlessAnalysisCommand::Failure;
    }

    emit_status(options,
                "failed",
                "Headless categorization failed.",
                "Unknown analysis result.",
                metadata,
                out,
                err);
    return HeadlessAnalysisCommand::Failure;
}

int finish_review_required_run(const HeadlessAnalysisCommand::Options& options,
                               const HeadlessReviewApplyService::Result& apply_result,
                               const std::filesystem::path& review_file,
                               const AnalysisRuntimeLock::Metadata& metadata,
                               std::ostream& out,
                               std::ostream& err)
{
    std::ostringstream message;
    message << "Headless " << HeadlessAnalysisCommand::operation_to_string(options.operation)
            << " prepared review. Review entries: " << apply_result.planned_count
            << ". No files were moved or renamed.";
    QJsonObject payload = apply_result_to_json(apply_result, true);
    payload.insert(QStringLiteral("reviewFile"),
                   QString::fromStdString(Utils::path_to_utf8(review_file)));
    emit_status(options, "review_required", message.str(), {}, metadata, out, err, &payload);
    return HeadlessAnalysisCommand::Success;
}

HeadlessAnalysisCommand::Options status_options_for_plan(
    const HeadlessAnalysisCommand::Options& options,
    const HeadlessReviewPlan& plan)
{
    HeadlessAnalysisCommand::Options status_options = options;
    if (status_options.operation == HeadlessAnalysisCommand::Operation::Unknown) {
        status_options.operation = plan.operation;
    }
    if (status_options.paths.empty()) {
        status_options.paths = plan.paths;
    }
    return status_options;
}

int finish_review_apply_run(const HeadlessAnalysisCommand::Options& status_options,
                            const HeadlessReviewApplyService::Result& apply_result,
                            const std::filesystem::path& review_file,
                            const AnalysisRuntimeLock::Metadata& metadata,
                            std::ostream& out,
                            std::ostream& err)
{
    std::ostringstream message;
    message << "Headless " << HeadlessAnalysisCommand::operation_to_string(status_options.operation)
            << " applied approved review. Review entries: " << apply_result.planned_count
            << ". Moved: " << apply_result.moved_count
            << ". Renamed: " << apply_result.renamed_count
            << ". Skipped: " << apply_result.skipped_count << ".";
    QJsonObject payload = apply_result_to_json(apply_result, false);
    payload.insert(QStringLiteral("reviewFile"),
                   QString::fromStdString(Utils::path_to_utf8(review_file)));
    emit_status(status_options, "completed", message.str(), {}, metadata, out, err, &payload);
    return HeadlessAnalysisCommand::Success;
}

int run_review_apply_plan(const HeadlessAnalysisCommand::Options& options,
                          const AnalysisRuntimeLock::Metadata& metadata,
                          std::ostream& out,
                          std::ostream& err)
{
    HeadlessReviewPlan plan;
    std::string read_error;
    if (!read_review_plan_file(*options.review_file, &plan, &read_error)) {
        emit_status(options,
                    "failed",
                    "Could not load approved headless review plan.",
                    read_error,
                    metadata,
                    out,
                    err);
        return HeadlessAnalysisCommand::Failure;
    }

    const HeadlessAnalysisCommand::Options status_options =
        status_options_for_plan(options, plan);
    emit_status(status_options,
                "running",
                "Applying approved headless review plan.",
                {},
                metadata,
                out,
                err);

    Settings settings;
    settings.load();
    DatabaseManager db_manager(settings.get_config_dir());
    ReviewHistoryStore history_store(settings.get_config_dir());
    LocalFsProvider storage_provider;
    HeadlessReviewApplyService apply_service(&db_manager,
                                             storage_provider,
                                             Logger::get_logger("core_logger"),
                                             &history_store);
    HeadlessReviewApplyService::Options apply_options = plan.apply_options;
    apply_options.apply_changes = true;
    const auto apply_result = apply_service.apply(plan.entries, apply_options);
    return finish_review_apply_run(status_options,
                                   apply_result,
                                   *options.review_file,
                                   metadata,
                                   out,
                                   err);
}

} // namespace

HeadlessAnalysisCommand::ParseResult HeadlessAnalysisCommand::parse(int argc, char** argv)
{
    ParseResult result;
    result.consumed_arguments.assign(static_cast<std::size_t>(std::max(argc, 0)), false);
    if (argc <= 1 || !argv) {
        return result;
    }

    for (int i = 1; i < argc; ++i) {
        if (!argv[i]) {
            continue;
        }
        if (is_headless_request_flag(argv[i])) {
            result.requested = true;
            break;
        }
    }

    if (!result.requested) {
        return result;
    }

    for (int i = 1; i < argc; ++i) {
        if (!argv[i]) {
            continue;
        }

        const std::string argument = argv[i];
        if (argument == "--headless") {
            result.consumed_arguments[static_cast<std::size_t>(i)] = true;
            continue;
        }
        if (argument == "--headless-apply") {
            result.options.request_mode = RequestMode::ApplyReview;
            result.consumed_arguments[static_cast<std::size_t>(i)] = true;
            continue;
        }
        if (argument == "--headless-help") {
            result.help_requested = true;
            result.consumed_arguments[static_cast<std::size_t>(i)] = true;
            continue;
        }
        if (argument == "--review-only" || argument == "--headless-review-only" ||
            argument == "--require-review" || argument == "--headless-require-review") {
            result.options.apply_mode = ApplyMode::ReviewOnly;
            result.consumed_arguments[static_cast<std::size_t>(i)] = true;
            continue;
        }
        if (argument == "--auto-apply" || argument == "--headless-auto-apply" ||
            argument == "--apply-without-review" || argument == "--headless-apply-without-review") {
            result.options.apply_mode = ApplyMode::AutoApply;
            result.consumed_arguments[static_cast<std::size_t>(i)] = true;
            continue;
        }
        if (argument == "--include-subdirectories" ||
            argument == "--headless-include-subdirectories") {
            result.options.include_subdirectories = true;
            result.consumed_arguments[static_cast<std::size_t>(i)] = true;
            continue;
        }
        if (argument == "--no-include-subdirectories" ||
            argument == "--headless-no-include-subdirectories") {
            result.options.include_subdirectories = false;
            result.consumed_arguments[static_cast<std::size_t>(i)] = true;
            continue;
        }

        std::string value;
        if (parse_value(argc, argv, i, argument, "--operation", "--headless-operation", &value, &result)) {
            if (result.error.empty()) {
                result.options.operation = operation_from_string(value);
                if (result.options.operation == Operation::Unknown) {
                    result.error = "Unsupported headless operation: " + value + ".";
                }
            }
            continue;
        }
        if (parse_value(argc, argv, i, argument, "--path", "--headless-path", &value, &result)) {
            if (result.error.empty()) {
                result.options.paths.push_back(Utils::utf8_to_path(value));
            }
            continue;
        }
        if (parse_value(argc, argv, i, argument, "--status-file", "--headless-status-file", &value, &result)) {
            if (result.error.empty()) {
                result.options.status_file = Utils::utf8_to_path(value);
            }
            continue;
        }
        if (parse_value(argc, argv, i, argument, "--review-file", "--headless-review-file", &value, &result)) {
            if (result.error.empty()) {
                result.options.review_file = Utils::utf8_to_path(value);
            }
            continue;
        }
        if (parse_value(argc,
                        argv,
                        i,
                        argument,
                        "--settings-overrides-file",
                        "--headless-settings-overrides-file",
                        &value,
                        &result)) {
            if (result.error.empty()) {
                result.options.settings_overrides_file = Utils::utf8_to_path(value);
            }
            continue;
        }
        if (parse_value(argc, argv, i, argument, "--job-id", "--headless-job-id", &value, &result)) {
            if (result.error.empty()) {
                result.options.job_id = value;
            }
            continue;
        }

        if (argument_has_prefix(argument, "--headless-") ||
            argument_has_prefix(argument, "--operation=") ||
            argument_has_prefix(argument, "--path=") ||
            argument_has_prefix(argument, "--status-file=") ||
            argument_has_prefix(argument, "--review-file=") ||
            argument_has_prefix(argument, "--settings-overrides-file=") ||
            argument_has_prefix(argument, "--job-id=") ||
            argument_has_prefix(argument, "--include-subdirectories=") ||
            argument_has_prefix(argument, "--no-include-subdirectories=") ||
            is_value_argument(argument)) {
            result.consumed_arguments[static_cast<std::size_t>(i)] = true;
            if (result.error.empty()) {
                result.error = "Unrecognized headless argument: " + argument + ".";
            }
        }
    }

    if (result.options.job_id.empty()) {
        result.options.job_id = generate_job_id();
    }
    if (!result.help_requested && result.error.empty()) {
        if (auto validation_error = validate_options(result.options)) {
            result.error = *validation_error;
        }
    }
    return result;
}

std::string HeadlessAnalysisCommand::usage_text()
{
    return "Usage: aifilesorter --headless --operation <categorize|rename|categorize-and-rename> "
           "--path <file-or-folder> [--path <file-or-folder> ...] "
           "[--status-file <json-file>] [--job-id <id>] "
           "[--review-file <json-file>] [--review-only|--auto-apply] "
           "[--include-subdirectories|--no-include-subdirectories] "
           "[--settings-overrides-file <json-file>]\n"
           "       aifilesorter --headless-apply --review-file <json-file> "
           "[--status-file <json-file>] [--job-id <id>]\n";
}

std::string HeadlessAnalysisCommand::operation_to_string(Operation operation)
{
    switch (operation) {
    case Operation::Categorize:
        return "categorize";
    case Operation::Rename:
        return "rename";
    case Operation::CategorizeAndRename:
        return "categorize-and-rename";
    case Operation::Unknown:
    default:
        return "unknown";
    }
}

HeadlessAnalysisCommand::Operation
HeadlessAnalysisCommand::operation_from_string(const std::string& value)
{
    const std::string normalized = lower_copy(value);
    if (normalized == "categorize") {
        return Operation::Categorize;
    }
    if (normalized == "rename") {
        return Operation::Rename;
    }
    if (normalized == "categorize-and-rename" || normalized == "categorize_rename" ||
        normalized == "categorizeandrename") {
        return Operation::CategorizeAndRename;
    }
    return Operation::Unknown;
}

int HeadlessAnalysisCommand::run(const Options& options,
                                 const std::filesystem::path& runtime_dir,
                                 std::ostream& out,
                                 std::ostream& err)
{
    if (auto validation_error = validate_options(options)) {
        emit_status(options, "failed", "Invalid headless command.", *validation_error, std::nullopt, out, err);
        return ExitCode::Usage;
    }

    AnalysisRuntimeLock runtime_lock(runtime_dir);
    AnalysisRuntimeLock::Metadata lock_metadata;
    if (runtime_lock.is_locked(&lock_metadata)) {
        emit_status(options,
                    "blocked",
                    "Another analysis job is already running.",
                    {},
                    lock_metadata,
                    out,
                    err);
        return ExitCode::Busy;
    }

    AnalysisRuntimeLock::Metadata metadata;
    metadata.owner = AnalysisRuntimeLock::Owner::Headless;
    metadata.job_id = options.job_id;
    metadata.description = "Headless " + operation_to_string(options.operation);

    std::string lock_error;
    auto lease = runtime_lock.try_acquire(metadata, &lock_error);
    if (!lease) {
        emit_status(options,
                    "blocked",
                    "Could not acquire the shared analysis runtime lock.",
                    lock_error,
                    std::nullopt,
                    out,
                    err);
        return ExitCode::Busy;
    }

    emit_status(options,
                "running",
                "Headless command accepted and runtime lock acquired.",
                {},
                lease->metadata(),
                out,
                err);

    if (options.request_mode == RequestMode::ApplyReview) {
        return run_review_apply_plan(options, lease->metadata(), out, err);
    }

    std::string target_error;
    const auto target = resolve_headless_target(options, &target_error);
    if (!target) {
        emit_status(options,
                    "failed",
                    "Unsupported headless target.",
                    target_error,
                    lease->metadata(),
                    out,
                    err);
        return ExitCode::Unsupported;
    }

    HeadlessSettingsOverrides settings_overrides = options.settings_overrides;
    if (options.settings_overrides_file) {
        std::string overrides_error;
        if (!read_settings_overrides_file(*options.settings_overrides_file,
                                          &settings_overrides,
                                          &overrides_error)) {
            emit_status(options,
                        "failed",
                        "Could not load headless settings.",
                        overrides_error,
                        lease->metadata(),
                        out,
                        err);
            return ExitCode::Failure;
        }
    }

    HeadlessAnalysisWorkflowHost::Options host_options;
    host_options.folder_path = target->folder_path;
    host_options.selected_paths = target->selected_paths;
    host_options.operation_mode = host_operation_mode(options.operation);
    host_options.include_subdirectories = options.include_subdirectories;
    host_options.settings_overrides = settings_overrides;
    host_options.progress_callback = [&](const std::string& message) {
        emit_status(options, "running", message, {}, lease->metadata(), out, err);
    };

    HeadlessAnalysisWorkflowHost host(std::move(host_options));
    const AnalysisRunResult result = host.execute();
    std::optional<HeadlessReviewApplyService::Result> apply_result;
    if (result.status == AnalysisRunStatus::Completed) {
        Settings history_settings;
        history_settings.load();
        ReviewHistoryStore history_store(history_settings.get_config_dir());
        HeadlessReviewApplyService::Options apply_options;
        apply_options.base_dir = host.folder_path();
        apply_options.undo_dir = host.undo_dir();
        apply_options.use_subcategories = host.use_subcategories();
        apply_options.include_subdirectories = host.include_subdirectories();
        apply_options.apply_suggested_names = operation_applies_suggested_names(options.operation);
        apply_options.move_categorized_entries = operation_moves_categorized_entries(options.operation);
        apply_options.category_language = host.category_language();
        apply_options.apply_changes = should_apply_changes(options.apply_mode, host);

        HeadlessReviewApplyService apply_service(&host.db_manager(),
                                                 host.storage_provider(),
                                                 host.core_logger(),
                                                 &history_store);
        apply_result = apply_service.apply(host.review_entries(), apply_options);
        if (!apply_options.apply_changes && has_actionable_review_entries(*apply_result)) {
            const auto review_file = review_file_path_for(options, runtime_dir);
            if (!review_file) {
                emit_status(options,
                            "failed",
                            "Could not prepare headless review.",
                            "A review plan path could not be resolved.",
                            lease->metadata(),
                            out,
                            err);
                return ExitCode::Failure;
            }
            std::string review_plan_error;
            if (!write_review_plan_file(*review_file,
                                        options,
                                        host.review_entries(),
                                        apply_options,
                                        &review_plan_error)) {
                emit_status(options,
                            "failed",
                            "Could not write headless review plan.",
                            review_plan_error,
                            lease->metadata(),
                            out,
                            err);
                return ExitCode::Failure;
            }
            return finish_review_required_run(options,
                                              *apply_result,
                                              *review_file,
                                              lease->metadata(),
                                              out,
                                              err);
        }
    }
    return finish_analysis_run(options,
                               result,
                               apply_result,
                               lease->metadata(),
                               out,
                               err);
}
