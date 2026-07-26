#include "HeadlessAnalysisCommand.hpp"

#include "AnalysisRunResult.hpp"
#include "DatabaseManager.hpp"
#include "HeadlessAnalysisWorkflowHost.hpp"
#include "HeadlessReviewApplyService.hpp"
#include "HeadlessSettingsOverridesJson.hpp"
#include "HeadlessStatusJson.hpp"
#include "LocalFsProvider.hpp"
#include "Logger.hpp"
#include "ReviewHistoryStore.hpp"
#include "Settings.hpp"
#include "Utils.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <sstream>
#include <system_error>
#include <thread>
#include <utility>

namespace {

constexpr auto kStopRequestPollInterval = std::chrono::milliseconds(250);

using HeadlessReviewPlan = HeadlessStatusJson::ReviewPlan;
using HeadlessStatusJson::apply_result_to_json;
using HeadlessStatusJson::emit_status;
using HeadlessStatusJson::read_review_plan_file;
using HeadlessStatusJson::review_file_path_for;
using HeadlessStatusJson::write_review_plan_file;

QString to_qstring(const std::filesystem::path& path)
{
#ifdef _WIN32
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}

std::string lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::filesystem::path stop_request_file_for_status(const std::filesystem::path& status_file)
{
    std::filesystem::path marker = status_file;
    marker += ".stop";
    return marker;
}

bool stop_requested_for_status_file(const std::optional<std::filesystem::path>& status_file)
{
    if (!status_file || status_file->empty()) {
        return false;
    }
    std::error_code ec;
    return std::filesystem::exists(stop_request_file_for_status(*status_file), ec);
}

/**
 * @brief Polls the Explorer stop marker while the workflow is inside blocking analysis code.
 */
class StopRequestMonitor {
public:
    StopRequestMonitor(const HeadlessAnalysisCommand::Options& options,
                       HeadlessAnalysisWorkflowHost* host)
        : status_file_(options.status_file),
          host_(host)
    {
        if (!status_file_ || !host_) {
            return;
        }
        worker_ = std::thread([this]() { run(); });
    }

    StopRequestMonitor(const StopRequestMonitor&) = delete;
    StopRequestMonitor& operator=(const StopRequestMonitor&) = delete;

    ~StopRequestMonitor()
    {
        done_.store(true);
        if (worker_.joinable()) {
            worker_.join();
        }
    }

private:
    void run()
    {
        while (!done_.load()) {
            if (stop_requested_for_status_file(status_file_)) {
                host_->request_stop();
                return;
            }
            std::this_thread::sleep_for(kStopRequestPollInterval);
        }
    }

    std::optional<std::filesystem::path> status_file_;
    HeadlessAnalysisWorkflowHost* host_{nullptr};
    std::atomic<bool> done_{false};
    std::thread worker_;
};

bool message_indicates_llm_setup_required(const std::string& message)
{
    const std::string lower = lower_copy(message);
    const auto contains = [&](const char* needle) {
        return lower.find(needle) != std::string::npos;
    };

    return contains("llm is not selected") ||
           contains("api key is missing") ||
           (contains("missing") && contains("api key")) ||
           contains("credentials are missing") ||
           contains("selected custom api endpoint is missing") ||
           contains("selected custom llm is missing") ||
           contains("custom visual llm") ||
           contains("visual backend is missing required model") ||
           contains("required local llm model path is not configured") ||
           contains("failed to load model");
}

QJsonObject llm_setup_required_payload()
{
    QJsonObject payload;
    payload.insert(QStringLiteral("actionRequired"), QStringLiteral("select_llm"));
    payload.insert(QStringLiteral("actionLabel"), QStringLiteral("Select LLM"));
    return payload;
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

    *overrides = HeadlessSettingsOverridesJson::from_json(document.object());
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
        if (message_indicates_llm_setup_required(result.error_message)) {
            const QJsonObject payload = llm_setup_required_payload();
            emit_status(options,
                        "blocked",
                        "AI File Sorter needs an LLM selection before this Explorer job can run. "
                        "Choose or download an LLM, then run the Explorer action again.",
                        result.error_message,
                        metadata,
                        out,
                        err,
                        &payload);
            return HeadlessAnalysisCommand::Failure;
        }
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
                   HeadlessStatusJson::path_to_json_string(review_file));
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
                   HeadlessStatusJson::path_to_json_string(review_file));
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
    host_options.stop_requested = [&]() {
        return stop_requested_for_status_file(options.status_file);
    };

    HeadlessAnalysisWorkflowHost host(std::move(host_options));
    StopRequestMonitor stop_monitor(options, &host);
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
