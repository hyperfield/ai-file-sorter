#include "HeadlessStatusJson.hpp"

#include "CategoryLanguage.hpp"
#include "Utils.hpp"

#include <QByteArray>
#include <QDateTime>
#include <QFile>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QString>

#include <cstdlib>
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

QString from_utf8(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

std::string to_utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

std::string utc_now_iso()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();
}

QJsonObject metadata_to_json(const AnalysisRuntimeLock::Metadata& metadata)
{
    QJsonObject object;
    object.insert(QStringLiteral("owner"),
                  from_utf8(AnalysisRuntimeLock::owner_to_string(metadata.owner)));
    object.insert(QStringLiteral("pid"), QString::number(metadata.pid));
    object.insert(QStringLiteral("jobId"), from_utf8(metadata.job_id));
    object.insert(QStringLiteral("startedAtUtc"), from_utf8(metadata.started_at_utc));
    object.insert(QStringLiteral("description"), from_utf8(metadata.description));
    return object;
}

void insert_env_value(QJsonObject& object, const QString& key, const char* env_name)
{
    const char* value = std::getenv(env_name);
    if (!value || value[0] == '\0') {
        return;
    }
    object.insert(key, QString::fromLocal8Bit(value));
}

QJsonObject runtime_to_json()
{
    QJsonObject object;
    insert_env_value(object, QStringLiteral("gpuBackend"), "AI_FILE_SORTER_GPU_BACKEND");
    insert_env_value(object, QStringLiteral("ggmlDir"), "AI_FILE_SORTER_GGML_DIR");
    insert_env_value(object, QStringLiteral("llamaDevice"), "LLAMA_ARG_DEVICE");
    insert_env_value(object, QStringLiteral("ggmlDisableCuda"), "GGML_DISABLE_CUDA");
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
    object.insert(QStringLiteral("status"), from_utf8(status));
    object.insert(QStringLiteral("operation"),
                  from_utf8(HeadlessAnalysisCommand::operation_to_string(options.operation)));
    object.insert(QStringLiteral("jobId"), from_utf8(options.job_id));
    object.insert(QStringLiteral("message"), from_utf8(message));
    object.insert(QStringLiteral("error"), from_utf8(error));
    object.insert(QStringLiteral("updatedAtUtc"), from_utf8(utc_now_iso()));
    const QJsonObject runtime = runtime_to_json();
    if (!runtime.isEmpty()) {
        object.insert(QStringLiteral("runtime"), runtime);
    }

    QJsonArray paths;
    for (const auto& path : options.paths) {
        paths.append(HeadlessStatusJson::path_to_json_string(path));
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

bool write_json_file(const std::filesystem::path& path,
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
            *error = to_utf8(file.errorString());
        }
        return false;
    }

    const QByteArray payload = document.toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size()) {
        if (error) {
            *error = to_utf8(file.errorString());
        }
        return false;
    }
    if (!file.commit()) {
        if (error) {
            *error = to_utf8(file.errorString());
        }
        return false;
    }
    return true;
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
    object.insert(QStringLiteral("filePath"), from_utf8(entry.file_path));
    object.insert(QStringLiteral("fileName"), from_utf8(entry.file_name));
    object.insert(QStringLiteral("type"), file_type_to_json(entry.type));
    object.insert(QStringLiteral("category"), from_utf8(entry.category));
    object.insert(QStringLiteral("subcategory"), from_utf8(entry.subcategory));
    object.insert(QStringLiteral("taxonomyId"), entry.taxonomy_id);
    object.insert(QStringLiteral("fromCache"), entry.from_cache);
    object.insert(QStringLiteral("usedConsistencyHints"), entry.used_consistency_hints);
    object.insert(QStringLiteral("suggestedName"), from_utf8(entry.suggested_name));
    object.insert(QStringLiteral("renameOnly"), entry.rename_only);
    object.insert(QStringLiteral("renameApplied"), entry.rename_applied);
    object.insert(QStringLiteral("canonicalCategory"), from_utf8(entry.canonical_category));
    object.insert(QStringLiteral("canonicalSubcategory"), from_utf8(entry.canonical_subcategory));
    object.insert(QStringLiteral("learningContext"), from_utf8(entry.learning_context));
    object.insert(QStringLiteral("targetFolder"), from_utf8(entry.target_folder_relative_path));
    object.insert(QStringLiteral("folderTreeMode"), entry.folder_tree_mode);
    object.insert(QStringLiteral("targetFolderSuggestedNew"), entry.target_folder_suggested_new);
    object.insert(QStringLiteral("targetFolderExists"), entry.target_folder_exists);
    object.insert(QStringLiteral("folderTreeAllowNewFolders"), entry.folder_tree_allow_new_folders);
    return object;
}

CategorizedFile categorized_file_from_json(const QJsonObject& object)
{
    CategorizedFile entry;
    entry.file_path = to_utf8(object.value(QStringLiteral("filePath")).toString());
    entry.file_name = to_utf8(object.value(QStringLiteral("fileName")).toString());
    entry.type = file_type_from_json(object.value(QStringLiteral("type")).toString());
    entry.category = to_utf8(object.value(QStringLiteral("category")).toString());
    entry.subcategory = to_utf8(object.value(QStringLiteral("subcategory")).toString());
    entry.taxonomy_id = object.value(QStringLiteral("taxonomyId")).toInt();
    entry.from_cache = object.value(QStringLiteral("fromCache")).toBool();
    entry.used_consistency_hints = object.value(QStringLiteral("usedConsistencyHints")).toBool();
    entry.suggested_name = to_utf8(object.value(QStringLiteral("suggestedName")).toString());
    entry.rename_only = object.value(QStringLiteral("renameOnly")).toBool();
    entry.rename_applied = object.value(QStringLiteral("renameApplied")).toBool();
    entry.canonical_category = to_utf8(object.value(QStringLiteral("canonicalCategory")).toString());
    entry.canonical_subcategory = to_utf8(object.value(QStringLiteral("canonicalSubcategory")).toString());
    entry.learning_context = to_utf8(object.value(QStringLiteral("learningContext")).toString());
    entry.target_folder_relative_path = to_utf8(object.value(QStringLiteral("targetFolder")).toString());
    entry.folder_tree_mode = object.value(QStringLiteral("folderTreeMode")).toBool(false);
    entry.target_folder_suggested_new =
        object.value(QStringLiteral("targetFolderSuggestedNew")).toBool(false);
    entry.target_folder_exists = object.value(QStringLiteral("targetFolderExists")).toBool(false);
    entry.folder_tree_allow_new_folders =
        object.value(QStringLiteral("folderTreeAllowNewFolders")).toBool(false);
    return entry;
}

QJsonObject apply_options_to_json(const HeadlessReviewApplyService::Options& options)
{
    QJsonObject object;
    object.insert(QStringLiteral("baseDir"), from_utf8(options.base_dir));
    object.insert(QStringLiteral("undoDir"), from_utf8(options.undo_dir));
    object.insert(QStringLiteral("useSubcategories"), options.use_subcategories);
    object.insert(QStringLiteral("includeSubdirectories"), options.include_subdirectories);
    object.insert(QStringLiteral("applySuggestedNames"), options.apply_suggested_names);
    object.insert(QStringLiteral("moveCategorizedEntries"), options.move_categorized_entries);
    object.insert(QStringLiteral("allowNewFolderTargets"), options.allow_new_folder_targets);
    object.insert(QStringLiteral("categoryLanguage"), categoryLanguageToString(options.category_language));
    return object;
}

HeadlessReviewApplyService::Options apply_options_from_json(const QJsonObject& object)
{
    HeadlessReviewApplyService::Options options;
    options.base_dir = to_utf8(object.value(QStringLiteral("baseDir")).toString());
    options.undo_dir = to_utf8(object.value(QStringLiteral("undoDir")).toString());
    options.use_subcategories = object.value(QStringLiteral("useSubcategories")).toBool(true);
    options.include_subdirectories = object.value(QStringLiteral("includeSubdirectories")).toBool(false);
    options.apply_suggested_names = object.value(QStringLiteral("applySuggestedNames")).toBool(false);
    options.move_categorized_entries = object.value(QStringLiteral("moveCategorizedEntries")).toBool(true);
    options.allow_new_folder_targets = object.value(QStringLiteral("allowNewFolderTargets")).toBool(false);
    options.category_language =
        categoryLanguageFromString(object.value(QStringLiteral("categoryLanguage")).toString());
    options.apply_changes = true;
    return options;
}

QJsonObject review_plan_to_json(const HeadlessAnalysisCommand::Options& options,
                                const std::vector<CategorizedFile>& entries,
                                const HeadlessReviewApplyService::Options& apply_options)
{
    QJsonArray paths;
    for (const auto& path : options.paths) {
        paths.append(HeadlessStatusJson::path_to_json_string(path));
    }

    QJsonArray serialized_entries;
    for (const auto& entry : entries) {
        serialized_entries.append(categorized_file_to_json(entry));
    }

    QJsonObject object;
    object.insert(QStringLiteral("schemaVersion"), kReviewPlanSchemaVersion);
    object.insert(QStringLiteral("kind"), QString::fromLatin1(kReviewPlanKind));
    object.insert(QStringLiteral("createdAtUtc"), from_utf8(utc_now_iso()));
    object.insert(QStringLiteral("jobId"), from_utf8(options.job_id));
    object.insert(QStringLiteral("operation"),
                  from_utf8(HeadlessAnalysisCommand::operation_to_string(options.operation)));
    object.insert(QStringLiteral("paths"), paths);
    object.insert(QStringLiteral("applyOptions"), apply_options_to_json(apply_options));
    object.insert(QStringLiteral("entries"), serialized_entries);
    return object;
}

} // namespace

QString HeadlessStatusJson::path_to_json_string(const std::filesystem::path& path)
{
    return from_utf8(Utils::path_to_utf8(path));
}

QJsonObject HeadlessStatusJson::apply_result_to_json(
    const HeadlessReviewApplyService::Result& result)
{
    QJsonArray entries;
    for (const auto& entry : result.entries) {
        QJsonObject object;
        object.insert(QStringLiteral("source"), from_utf8(entry.source));
        object.insert(QStringLiteral("destination"), from_utf8(entry.destination));
        object.insert(QStringLiteral("fileName"), from_utf8(entry.file_name));
        object.insert(QStringLiteral("destinationName"), from_utf8(entry.destination_name));
        object.insert(QStringLiteral("category"), from_utf8(entry.category));
        object.insert(QStringLiteral("subcategory"), from_utf8(entry.subcategory));
        object.insert(QStringLiteral("targetFolder"), from_utf8(entry.target_folder));
        object.insert(QStringLiteral("targetFolderSuggestedNew"), entry.target_folder_suggested_new);
        object.insert(QStringLiteral("targetFolderExists"), entry.target_folder_exists);
        object.insert(QStringLiteral("message"), from_utf8(entry.message));
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
    payload.insert(QStringLiteral("entryCount"), static_cast<qint64>(result.planned_count));
    payload.insert(QStringLiteral("entries"), entries);
    payload.insert(QStringLiteral("movedCount"), static_cast<qint64>(result.moved_count));
    payload.insert(QStringLiteral("renamedCount"), static_cast<qint64>(result.renamed_count));
    payload.insert(QStringLiteral("skippedCount"), static_cast<qint64>(result.skipped_count));
    payload.insert(QStringLiteral("review"), review);
    payload.insert(QStringLiteral("apply"), apply);
    return payload;
}

QJsonObject HeadlessStatusJson::apply_result_to_json(
    const HeadlessReviewApplyService::Result& result,
    bool review_required)
{
    QJsonObject payload = apply_result_to_json(result);
    payload.insert(QStringLiteral("reviewRequired"), review_required);
    QJsonObject review = payload.value(QStringLiteral("review")).toObject();
    review.insert(QStringLiteral("requiresApproval"), review_required);
    payload.insert(QStringLiteral("review"), review);
    return payload;
}

bool HeadlessStatusJson::emit_status(
    const HeadlessAnalysisCommand::Options& options,
    const std::string& status,
    const std::string& message,
    const std::string& error,
    const std::optional<AnalysisRuntimeLock::Metadata>& lock_metadata,
    std::ostream& out,
    std::ostream& err,
    const QJsonObject* extra_payload)
{
    const QJsonDocument document(
        status_to_json(options, status, message, error, lock_metadata, extra_payload));
    out << document.toJson(QJsonDocument::Compact).constData() << '\n';

    if (!options.status_file) {
        return true;
    }

    std::string file_error;
    if (!write_json_file(*options.status_file, document, &file_error)) {
        err << "Could not write headless status file: " << file_error << '\n';
        return false;
    }
    return true;
}

std::optional<std::filesystem::path>
HeadlessStatusJson::review_file_path_for(const HeadlessAnalysisCommand::Options& options,
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

bool HeadlessStatusJson::write_review_plan_file(
    const std::filesystem::path& path,
    const HeadlessAnalysisCommand::Options& options,
    const std::vector<CategorizedFile>& entries,
    const HeadlessReviewApplyService::Options& apply_options,
    std::string* error)
{
    const QJsonDocument document(review_plan_to_json(options, entries, apply_options));
    return write_json_file(path, document, error);
}

bool HeadlessStatusJson::read_review_plan_file(const std::filesystem::path& path,
                                               ReviewPlan* plan,
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
            *error = "Could not open review plan: " + to_utf8(file.errorString());
        }
        return false;
    }

    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) {
            *error = "Review plan is not valid JSON: " + to_utf8(parse_error.errorString());
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

    ReviewPlan parsed;
    parsed.operation =
        HeadlessAnalysisCommand::operation_from_string(
            to_utf8(object.value(QStringLiteral("operation")).toString()));
    if (parsed.operation == HeadlessAnalysisCommand::Operation::Unknown) {
        if (error) {
            *error = "Review plan operation is missing or unsupported.";
        }
        return false;
    }

    for (const auto& value : object.value(QStringLiteral("paths")).toArray()) {
        parsed.paths.push_back(Utils::utf8_to_path(to_utf8(value.toString())));
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
