#include "HeadlessAnalysisCommand.hpp"

#include "AnalysisRunResult.hpp"
#include "HeadlessAnalysisWorkflowHost.hpp"
#include "HeadlessReviewApplyService.hpp"
#include "Utils.hpp"

#include <QCoreApplication>
#include <QDateTime>
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
    return argument == "--headless" || argument == "--headless-help";
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
           argument == "--job-id" || argument == "--headless-job-id";
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

std::optional<std::string> validate_options(const HeadlessAnalysisCommand::Options& options)
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

bool is_directory_target(const std::filesystem::path& path)
{
    std::error_code ec;
    return std::filesystem::is_directory(path, ec) && !ec;
}

std::optional<std::filesystem::path>
resolve_single_folder_target(const HeadlessAnalysisCommand::Options& options,
                             std::string* error)
{
    if (options.paths.size() != 1) {
        if (error) {
            *error = "This build supports one folder target per headless categorization job.";
        }
        return std::nullopt;
    }
    if (!is_directory_target(options.paths.front())) {
        if (error) {
            *error = "This build supports folder targets for headless categorization. "
                     "File and multi-select jobs will be added in a later plugin slice.";
        }
        return std::nullopt;
    }
    return options.paths.front();
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
        message << "Headless categorization completed. Review entries: " << review_entry_count << ".";
        if (apply_result) {
            message << " Moved: " << apply_result->moved_count
                    << ". Skipped: " << apply_result->skipped_count << ".";
            const QJsonObject payload = apply_result_to_json(*apply_result);
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
        if (argument == "--headless-help") {
            result.help_requested = true;
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
            argument_has_prefix(argument, "--job-id=") ||
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

    if (options.operation != Operation::Categorize) {
        emit_status(options,
                    "failed",
                    "Headless execution is not implemented for this operation yet.",
                    "This build can run folder categorization. Rename and categorize-and-rename still need "
                    "rename-specific headless apply rules.",
                    lease->metadata(),
                    out,
                    err);
        return ExitCode::Unsupported;
    }

    std::string target_error;
    const auto folder_target = resolve_single_folder_target(options, &target_error);
    if (!folder_target) {
        emit_status(options,
                    "failed",
                    "Unsupported headless target.",
                    target_error,
                    lease->metadata(),
                    out,
                    err);
        return ExitCode::Unsupported;
    }

    HeadlessAnalysisWorkflowHost::Options host_options;
    host_options.folder_path = *folder_target;
    host_options.progress_callback = [&](const std::string& message) {
        emit_status(options, "running", message, {}, lease->metadata(), out, err);
    };

    HeadlessAnalysisWorkflowHost host(std::move(host_options));
    const AnalysisRunResult result = host.execute();
    std::optional<HeadlessReviewApplyService::Result> apply_result;
    if (result.status == AnalysisRunStatus::Completed) {
        HeadlessReviewApplyService::Options apply_options;
        apply_options.base_dir = host.folder_path();
        apply_options.undo_dir = host.undo_dir();
        apply_options.use_subcategories = host.use_subcategories();
        apply_options.include_subdirectories = host.include_subdirectories();
        apply_options.apply_suggested_names = false;
        apply_options.category_language = host.category_language();

        HeadlessReviewApplyService apply_service(&host.db_manager(),
                                                 host.storage_provider(),
                                                 host.core_logger());
        apply_result = apply_service.apply(host.review_entries(), apply_options);
    }
    return finish_analysis_run(options,
                               result,
                               apply_result,
                               lease->metadata(),
                               out,
                               err);
}
