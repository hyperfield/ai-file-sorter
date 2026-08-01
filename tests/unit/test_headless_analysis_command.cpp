#include <catch2/catch_test_macros.hpp>

#include "AnalysisRuntimeLock.hpp"
#include "DatabaseManager.hpp"
#include "HeadlessAnalysisCommand.hpp"
#include "HeadlessReviewApplyService.hpp"
#include "LocalFsProvider.hpp"
#include "Settings.hpp"
#include "Utils.hpp"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtGlobal>

#include <algorithm>
#include <sstream>
#include <vector>

namespace {

class ScopedEnvironmentVariable {
public:
    ScopedEnvironmentVariable(const char* key, const QByteArray& value)
        : key_(key),
          had_value_(qEnvironmentVariableIsSet(key))
    {
        if (had_value_) {
            old_value_ = qgetenv(key);
        }
        qputenv(key_.constData(), value);
    }

    ~ScopedEnvironmentVariable()
    {
        if (had_value_) {
            qputenv(key_.constData(), old_value_);
        } else {
            qunsetenv(key_.constData());
        }
    }

private:
    QByteArray key_;
    QByteArray old_value_;
    bool had_value_{false};
};

std::filesystem::path make_file(const QTemporaryDir& dir, const QString& name)
{
    const QString path = dir.filePath(name);
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    REQUIRE(file.write("sample") == 6);
    file.close();
    return std::filesystem::path(path.toStdString());
}

QJsonObject read_status(const std::filesystem::path& path)
{
    QFile file(QString::fromStdString(path.string()));
    REQUIRE(file.open(QIODevice::ReadOnly));
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    REQUIRE(doc.isObject());
    return doc.object();
}

std::vector<QJsonObject> parse_status_stream(const std::string& stream)
{
    std::vector<QJsonObject> statuses;
    std::istringstream input(stream);
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(line));
        if (doc.isObject()) {
            statuses.push_back(doc.object());
        }
    }
    return statuses;
}

std::filesystem::path make_file_at(const std::filesystem::path& path)
{
    const std::string utf8_path = Utils::path_to_utf8(path);
    QFile file(QString::fromUtf8(utf8_path.data(), static_cast<qsizetype>(utf8_path.size())));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    REQUIRE(file.write("sample") == 6);
    file.close();
    return path;
}

std::string normalized_path_key(const std::filesystem::path& path)
{
    return Utils::path_to_utf8(std::filesystem::absolute(path).lexically_normal());
}

void cache_categorization(DatabaseManager& db,
                          const std::filesystem::path& directory,
                          const std::string& file_name,
                          const std::string& category,
                          const std::string& subcategory,
                          const std::string& suggested_name = {})
{
    const auto resolved = db.resolve_category(category, subcategory);
    REQUIRE(db.insert_or_update_file_with_categorization(file_name,
                                                         "F",
                                                         normalized_path_key(directory),
                                                         resolved,
                                                         false,
                                                         suggested_name));
}

std::string utf8_review_filename()
{
    return std::string("rapport_") +
           "\xC3\xA9" +
           "nergie_" +
           "\xE4\xBC\x9A\xE8\xAE\xAE" +
           "_" +
           "\xE0\xA4\x85\xE0\xA4\xA8\xE0\xA5\x81\xE0\xA4\xB8\xE0\xA4\x82"
           "\xE0\xA4\xA7\xE0\xA4\xBE\xE0\xA4\xA8" +
           ".txt";
}

void enable_auto_apply(HeadlessAnalysisCommand::Options& options)
{
    options.apply_mode = HeadlessAnalysisCommand::ApplyMode::AutoApply;
}

} // namespace

TEST_CASE("HeadlessAnalysisCommand parses operation paths and status file")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const std::filesystem::path target = make_file(dir, QStringLiteral("input.txt"));
    const std::filesystem::path status = std::filesystem::path(dir.path().toStdString()) / "status.json";

    std::string target_arg = target.string();
    std::string status_arg = status.string();
    char arg0[] = "aifilesorter";
    char arg1[] = "--headless";
    char arg2[] = "--operation";
    char arg3[] = "categorize-and-rename";
    char arg4[] = "--path";
    char arg6[] = "--status-file";
    char arg8[] = "--job-id=test-job";
    char* argv[] = {arg0, arg1, arg2, arg3, arg4, target_arg.data(), arg6, status_arg.data(), arg8};

    const auto parsed = HeadlessAnalysisCommand::parse(9, argv);
    REQUIRE(parsed.requested);
    CHECK_FALSE(parsed.help_requested);
    CHECK(parsed.error.empty());
    CHECK(parsed.options.request_mode == HeadlessAnalysisCommand::RequestMode::Analyze);
    CHECK(parsed.options.operation == HeadlessAnalysisCommand::Operation::CategorizeAndRename);
    CHECK(parsed.options.apply_mode == HeadlessAnalysisCommand::ApplyMode::UseSettings);
    REQUIRE(parsed.options.paths.size() == 1);
    CHECK(parsed.options.paths.front() == target);
    REQUIRE(parsed.options.status_file.has_value());
    CHECK(*parsed.options.status_file == status);
    CHECK(parsed.options.job_id == "test-job");
}

TEST_CASE("HeadlessAnalysisCommand parses apply mode flags")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const std::filesystem::path target = make_file(dir, QStringLiteral("input.txt"));

    std::string target_arg = target.string();
    char arg0[] = "aifilesorter";
    char arg1[] = "--headless";
    char arg2[] = "--operation=rename";
    char arg3[] = "--path";
    char arg5[] = "--review-only";
    char* review_argv[] = {arg0, arg1, arg2, arg3, target_arg.data(), arg5};

    const auto review_parsed = HeadlessAnalysisCommand::parse(6, review_argv);
    REQUIRE(review_parsed.requested);
    CHECK(review_parsed.error.empty());
    CHECK(review_parsed.options.apply_mode == HeadlessAnalysisCommand::ApplyMode::ReviewOnly);

    char arg6[] = "--headless-auto-apply";
    char* apply_argv[] = {arg0, arg1, arg2, arg3, target_arg.data(), arg6};
    const auto apply_parsed = HeadlessAnalysisCommand::parse(6, apply_argv);
    REQUIRE(apply_parsed.requested);
    CHECK(apply_parsed.error.empty());
    CHECK(apply_parsed.options.apply_mode == HeadlessAnalysisCommand::ApplyMode::AutoApply);
}

TEST_CASE("HeadlessAnalysisCommand parses include subdirectories override")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const std::filesystem::path target = make_file(dir, QStringLiteral("input.txt"));

    std::string target_arg = target.string();
    char arg0[] = "aifilesorter";
    char arg1[] = "--headless";
    char arg2[] = "--operation=rename";
    char arg3[] = "--path";
    char arg5[] = "--include-subdirectories";
    char* include_argv[] = {arg0, arg1, arg2, arg3, target_arg.data(), arg5};

    const auto include_parsed = HeadlessAnalysisCommand::parse(6, include_argv);
    REQUIRE(include_parsed.requested);
    CHECK(include_parsed.error.empty());
    REQUIRE(include_parsed.options.include_subdirectories.has_value());
    CHECK(*include_parsed.options.include_subdirectories);

    char arg6[] = "--headless-no-include-subdirectories";
    char* exclude_argv[] = {arg0, arg1, arg2, arg3, target_arg.data(), arg6};
    const auto exclude_parsed = HeadlessAnalysisCommand::parse(6, exclude_argv);
    REQUIRE(exclude_parsed.requested);
    CHECK(exclude_parsed.error.empty());
    REQUIRE(exclude_parsed.options.include_subdirectories.has_value());
    CHECK_FALSE(*exclude_parsed.options.include_subdirectories);
}

TEST_CASE("HeadlessAnalysisCommand parses settings overrides file")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const std::filesystem::path target = make_file(dir, QStringLiteral("input.txt"));
    const std::filesystem::path overrides =
        std::filesystem::path(dir.path().toStdString()) / "settings-overrides.json";

    std::string target_arg = target.string();
    std::string overrides_arg = overrides.string();
    char arg0[] = "aifilesorter";
    char arg1[] = "--headless";
    char arg2[] = "--operation=categorize";
    char arg3[] = "--path";
    char arg5[] = "--settings-overrides-file";
    char* argv[] = {arg0, arg1, arg2, arg3, target_arg.data(), arg5, overrides_arg.data()};

    const auto parsed = HeadlessAnalysisCommand::parse(7, argv);
    REQUIRE(parsed.requested);
    CHECK(parsed.error.empty());
    REQUIRE(parsed.options.settings_overrides_file.has_value());
    CHECK(*parsed.options.settings_overrides_file == overrides);
}

TEST_CASE("HeadlessAnalysisCommand parses saved review apply requests")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const std::filesystem::path review_file = make_file(dir, QStringLiteral("review.json"));
    const std::filesystem::path status = std::filesystem::path(dir.path().toStdString()) / "status.json";

    std::string review_arg = review_file.string();
    std::string status_arg = status.string();
    char arg0[] = "aifilesorter";
    char arg1[] = "--headless-apply";
    char arg2[] = "--review-file";
    char arg4[] = "--status-file";
    char arg6[] = "--job-id=apply-job";
    char* argv[] = {arg0, arg1, arg2, review_arg.data(), arg4, status_arg.data(), arg6};

    const auto parsed = HeadlessAnalysisCommand::parse(7, argv);
    REQUIRE(parsed.requested);
    CHECK(parsed.error.empty());
    CHECK(parsed.options.request_mode == HeadlessAnalysisCommand::RequestMode::ApplyReview);
    REQUIRE(parsed.options.review_file.has_value());
    CHECK(*parsed.options.review_file == review_file);
    REQUIRE(parsed.options.status_file.has_value());
    CHECK(*parsed.options.status_file == status);
    CHECK(parsed.options.job_id == "apply-job");
}

TEST_CASE("HeadlessAnalysisCommand reports busy runtime lock")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const std::filesystem::path target = make_file(dir, QStringLiteral("input.txt"));
    const std::filesystem::path runtime_dir = std::filesystem::path(dir.path().toStdString()) / "runtime";
    const std::filesystem::path status = std::filesystem::path(dir.path().toStdString()) / "status.json";

    AnalysisRuntimeLock lock(runtime_dir);
    AnalysisRuntimeLock::Metadata metadata;
    metadata.owner = AnalysisRuntimeLock::Owner::ExplorerWorker;
    metadata.job_id = "explorer-job";
    metadata.description = "Explorer analysis";
    auto lease = lock.try_acquire(metadata);
    REQUIRE(lease.has_value());

    HeadlessAnalysisCommand::Options options;
    options.operation = HeadlessAnalysisCommand::Operation::Categorize;
    options.paths.push_back(target);
    options.status_file = status;
    options.job_id = "headless-job";

    std::ostringstream out;
    std::ostringstream err;
    const int exit_code = HeadlessAnalysisCommand::run(options, runtime_dir, out, err);

    CHECK(exit_code == HeadlessAnalysisCommand::Busy);
    const QJsonObject object = read_status(status);
    CHECK(object.value(QStringLiteral("status")).toString() == QStringLiteral("blocked"));
    CHECK(object.value(QStringLiteral("lock")).toObject().value(QStringLiteral("owner")).toString() ==
          QStringLiteral("explorerWorker"));
    CHECK(out.str().find("\"blocked\"") != std::string::npos);
}

TEST_CASE("HeadlessAnalysisCommand runs categorization for an empty folder")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QTemporaryDir config_root;
    REQUIRE(config_root.isValid());
    ScopedEnvironmentVariable config_env("AI_FILE_SORTER_CONFIG_DIR",
                                         config_root.path().toUtf8());

    const QString target_path = dir.filePath(QStringLiteral("target"));
    REQUIRE(QDir().mkpath(target_path));
    const std::filesystem::path target = std::filesystem::path(target_path.toStdString());
    const std::filesystem::path runtime_dir = std::filesystem::path(dir.path().toStdString()) / "runtime";
    const std::filesystem::path status = std::filesystem::path(dir.path().toStdString()) / "status.json";

    HeadlessAnalysisCommand::Options options;
    options.operation = HeadlessAnalysisCommand::Operation::Categorize;
    options.paths.push_back(target);
    options.status_file = status;
    options.job_id = "headless-empty-folder";

    std::ostringstream out;
    std::ostringstream err;
    const int exit_code = HeadlessAnalysisCommand::run(options, runtime_dir, out, err);

    CHECK(exit_code == HeadlessAnalysisCommand::Success);
    const QJsonObject object = read_status(status);
    CHECK(object.value(QStringLiteral("status")).toString() == QStringLiteral("completed"));
    CHECK(object.value(QStringLiteral("operation")).toString() == QStringLiteral("categorize"));
    CHECK(object.value(QStringLiteral("message")).toString().contains(QStringLiteral("Review entries: 0")));
    CHECK(out.str().find("\"completed\"") != std::string::npos);

    AnalysisRuntimeLock lock(runtime_dir);
    CHECK_FALSE(lock.is_locked());
}

TEST_CASE("HeadlessAnalysisCommand honors stop marker for headless jobs")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QTemporaryDir config_root;
    REQUIRE(config_root.isValid());
    ScopedEnvironmentVariable config_env("AI_FILE_SORTER_CONFIG_DIR",
                                         config_root.path().toUtf8());

    const QString target_path = dir.filePath(QStringLiteral("target"));
    REQUIRE(QDir().mkpath(target_path));
    const std::filesystem::path target = std::filesystem::path(target_path.toStdString());
    const std::filesystem::path runtime_dir = std::filesystem::path(dir.path().toStdString()) / "runtime";
    const std::filesystem::path status = std::filesystem::path(dir.path().toStdString()) / "status.json";
    std::filesystem::path stop_marker = status;
    stop_marker += ".stop";
    QFile marker(QString::fromStdString(Utils::path_to_utf8(stop_marker)));
    REQUIRE(marker.open(QIODevice::WriteOnly | QIODevice::Truncate));
    REQUIRE(marker.write("stop") == 4);
    marker.close();

    HeadlessAnalysisCommand::Options options;
    options.operation = HeadlessAnalysisCommand::Operation::Categorize;
    options.paths.push_back(target);
    options.status_file = status;
    options.job_id = "headless-stop-marker";

    std::ostringstream out;
    std::ostringstream err;
    const int exit_code = HeadlessAnalysisCommand::run(options, runtime_dir, out, err);

    CHECK(exit_code == HeadlessAnalysisCommand::Failure);
    const QJsonObject object = read_status(status);
    CHECK(object.value(QStringLiteral("status")).toString() == QStringLiteral("cancelled"));
    CHECK(out.str().find("\"cancelled\"") != std::string::npos);

    AnalysisRuntimeLock lock(runtime_dir);
    CHECK_FALSE(lock.is_locked());
}

TEST_CASE("HeadlessAnalysisCommand reports LLM setup action when categorization has no LLM")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QTemporaryDir config_root;
    REQUIRE(config_root.isValid());
    ScopedEnvironmentVariable config_env("AI_FILE_SORTER_CONFIG_DIR",
                                         config_root.path().toUtf8());

    const std::filesystem::path target =
        std::filesystem::path(dir.filePath(QStringLiteral("target")).toStdString());
    REQUIRE(QDir().mkpath(QString::fromStdString(Utils::path_to_utf8(target))));
    const std::filesystem::path source = make_file_at(target / "input.txt");

    const std::filesystem::path runtime_dir = std::filesystem::path(dir.path().toStdString()) / "runtime";
    const std::filesystem::path status = std::filesystem::path(dir.path().toStdString()) / "status.json";

    HeadlessAnalysisCommand::Options options;
    options.operation = HeadlessAnalysisCommand::Operation::Categorize;
    options.paths.push_back(source);
    options.status_file = status;
    options.job_id = "headless-llm-setup-required";

    std::ostringstream out;
    std::ostringstream err;
    const int exit_code = HeadlessAnalysisCommand::run(options, runtime_dir, out, err);

    CHECK(exit_code == HeadlessAnalysisCommand::Failure);
    const QJsonObject object = read_status(status);
    CHECK(object.value(QStringLiteral("status")).toString() == QStringLiteral("blocked"));
    CHECK(object.value(QStringLiteral("actionRequired")).toString() == QStringLiteral("select_llm"));
    CHECK(object.value(QStringLiteral("actionLabel")).toString() == QStringLiteral("Select LLM"));
    CHECK(object.value(QStringLiteral("message")).toString().contains(QStringLiteral("LLM selection")));
    CHECK(object.value(QStringLiteral("error")).toString().contains(QStringLiteral("LLM is not selected")));
    CHECK(out.str().find("\"select_llm\"") != std::string::npos);

    AnalysisRuntimeLock lock(runtime_dir);
    CHECK_FALSE(lock.is_locked());
}

TEST_CASE("HeadlessAnalysisCommand rename does not require an LLM for uncached ordinary files")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QTemporaryDir config_root;
    REQUIRE(config_root.isValid());
    ScopedEnvironmentVariable config_env("AI_FILE_SORTER_CONFIG_DIR",
                                         config_root.path().toUtf8());

    const std::filesystem::path target =
        std::filesystem::path(dir.filePath(QStringLiteral("target")).toStdString());
    REQUIRE(QDir().mkpath(QString::fromStdString(Utils::path_to_utf8(target))));
    const std::filesystem::path source = make_file_at(target / "archive.bin");

    const std::filesystem::path runtime_dir = std::filesystem::path(dir.path().toStdString()) / "runtime";
    const std::filesystem::path status = std::filesystem::path(dir.path().toStdString()) / "status.json";

    HeadlessAnalysisCommand::Options options;
    options.operation = HeadlessAnalysisCommand::Operation::Rename;
    options.paths.push_back(source);
    options.status_file = status;
    options.job_id = "headless-rename-no-llm";

    std::ostringstream out;
    std::ostringstream err;
    const int exit_code = HeadlessAnalysisCommand::run(options, runtime_dir, out, err);

    CHECK(exit_code == HeadlessAnalysisCommand::Success);
    CHECK(QFile::exists(QString::fromStdString(Utils::path_to_utf8(source))));
    CHECK(out.str().find("LLM is not selected") == std::string::npos);

    const QJsonObject object = read_status(status);
    CHECK(object.value(QStringLiteral("status")).toString() == QStringLiteral("completed"));
    const QJsonObject review = object.value(QStringLiteral("review")).toObject();
    const QJsonObject apply = object.value(QStringLiteral("apply")).toObject();
    CHECK(review.value(QStringLiteral("entryCount")).toInt() == 0);
    CHECK(apply.value(QStringLiteral("movedCount")).toInt() == 0);
    CHECK(apply.value(QStringLiteral("renamedCount")).toInt() == 0);
    CHECK(apply.value(QStringLiteral("skippedCount")).toInt() == 0);
}

TEST_CASE("HeadlessAnalysisCommand applies cached categorization for a folder")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QTemporaryDir config_root;
    REQUIRE(config_root.isValid());
    ScopedEnvironmentVariable config_env("AI_FILE_SORTER_CONFIG_DIR",
                                         config_root.path().toUtf8());

    const std::filesystem::path target =
        std::filesystem::path(dir.filePath(QStringLiteral("target")).toStdString());
    REQUIRE(QDir().mkpath(QString::fromStdString(Utils::path_to_utf8(target))));
    const std::filesystem::path source = make_file_at(target / "input.txt");
    const std::filesystem::path destination = target / "Documents" / "Reports" / "input.txt";

    Settings settings;
    settings.set_headless_review_before_apply(false);
    REQUIRE(settings.save());
    DatabaseManager db(settings.get_config_dir());
    const std::string target_key = normalized_path_key(target);
    const auto resolved = db.resolve_category("Documents", "Reports");
    REQUIRE(db.insert_or_update_file_with_categorization("input.txt",
                                                         "F",
                                                         target_key,
                                                         resolved,
                                                         false));

    const std::filesystem::path runtime_dir = std::filesystem::path(dir.path().toStdString()) / "runtime";
    const std::filesystem::path status = std::filesystem::path(dir.path().toStdString()) / "status.json";

    HeadlessAnalysisCommand::Options options;
    options.operation = HeadlessAnalysisCommand::Operation::Categorize;
    options.paths.push_back(target);
    options.status_file = status;
    options.job_id = "headless-cached-folder";

    std::ostringstream out;
    std::ostringstream err;
    const int exit_code = HeadlessAnalysisCommand::run(options, runtime_dir, out, err);

    CHECK(exit_code == HeadlessAnalysisCommand::Success);
    CHECK_FALSE(QFile::exists(QString::fromStdString(Utils::path_to_utf8(source))));
    CHECK(QFile::exists(QString::fromStdString(Utils::path_to_utf8(destination))));

    const QJsonObject object = read_status(status);
    CHECK(object.value(QStringLiteral("status")).toString() == QStringLiteral("completed"));
    const QJsonObject review = object.value(QStringLiteral("review")).toObject();
    const QJsonObject apply = object.value(QStringLiteral("apply")).toObject();
    CHECK(review.value(QStringLiteral("entryCount")).toInt() == 1);
    CHECK(apply.value(QStringLiteral("movedCount")).toInt() == 1);
    CHECK(apply.value(QStringLiteral("skippedCount")).toInt() == 0);
    CHECK(apply.value(QStringLiteral("undoPlanSaved")).toBool());
    const QJsonArray entries = review.value(QStringLiteral("entries")).toArray();
    REQUIRE(entries.size() == 1);
    CHECK(entries.at(0).toObject().value(QStringLiteral("destination")).toString()
              .contains(QStringLiteral("Documents")));
}

TEST_CASE("HeadlessAnalysisCommand prepares review before applying by default")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QTemporaryDir config_root;
    REQUIRE(config_root.isValid());
    ScopedEnvironmentVariable config_env("AI_FILE_SORTER_CONFIG_DIR",
                                         config_root.path().toUtf8());

    const std::filesystem::path target =
        std::filesystem::path(dir.filePath(QStringLiteral("target")).toStdString());
    REQUIRE(QDir().mkpath(QString::fromStdString(Utils::path_to_utf8(target))));
    const std::filesystem::path source = make_file_at(target / "input.txt");
    const std::filesystem::path destination = target / "Documents" / "Reports" / "input.txt";

    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    cache_categorization(db, target, "input.txt", "Documents", "Reports");

    const std::filesystem::path runtime_dir = std::filesystem::path(dir.path().toStdString()) / "runtime";
    const std::filesystem::path status = std::filesystem::path(dir.path().toStdString()) / "status.json";

    HeadlessAnalysisCommand::Options options;
    options.operation = HeadlessAnalysisCommand::Operation::Categorize;
    options.paths.push_back(target);
    options.status_file = status;
    options.job_id = "headless-review-required";

    std::ostringstream out;
    std::ostringstream err;
    const int exit_code = HeadlessAnalysisCommand::run(options, runtime_dir, out, err);

    CHECK(exit_code == HeadlessAnalysisCommand::Success);
    CHECK(QFile::exists(QString::fromStdString(Utils::path_to_utf8(source))));
    CHECK_FALSE(QFile::exists(QString::fromStdString(Utils::path_to_utf8(destination))));
    CHECK(out.str().find("\"review_required\"") != std::string::npos);

    const QJsonObject object = read_status(status);
    CHECK(object.value(QStringLiteral("status")).toString() == QStringLiteral("review_required"));
    CHECK(object.value(QStringLiteral("reviewRequired")).toBool());
    const QString review_file = object.value(QStringLiteral("reviewFile")).toString();
    REQUIRE_FALSE(review_file.isEmpty());
    CHECK(QFile::exists(review_file));
    CHECK(object.value(QStringLiteral("entryCount")).toInt() == 1);
    CHECK(object.value(QStringLiteral("movedCount")).toInt() == 0);
    CHECK(object.value(QStringLiteral("renamedCount")).toInt() == 0);
    CHECK(object.value(QStringLiteral("skippedCount")).toInt() == 0);
    const QJsonObject review = object.value(QStringLiteral("review")).toObject();
    const QJsonObject apply = object.value(QStringLiteral("apply")).toObject();
    CHECK(review.value(QStringLiteral("requiresApproval")).toBool());
    CHECK(review.value(QStringLiteral("entryCount")).toInt() == 1);
    CHECK(apply.value(QStringLiteral("movedCount")).toInt() == 0);
    CHECK(apply.value(QStringLiteral("renamedCount")).toInt() == 0);
    CHECK(apply.value(QStringLiteral("skippedCount")).toInt() == 0);
    CHECK_FALSE(apply.value(QStringLiteral("undoPlanSaved")).toBool());
    const QJsonArray entries = review.value(QStringLiteral("entries")).toArray();
    REQUIRE(entries.size() == 1);
    const QJsonObject entry = entries.at(0).toObject();
    CHECK(entry.value(QStringLiteral("destination")).toString().contains(QStringLiteral("Documents")));
    CHECK(entry.value(QStringLiteral("message")).toString() == QStringLiteral("Waiting for review approval."));
    const QJsonArray flat_entries = object.value(QStringLiteral("entries")).toArray();
    REQUIRE(flat_entries.size() == 1);

    const auto statuses = parse_status_stream(out.str());
    const auto running_with_preview = std::find_if(
        statuses.begin(),
        statuses.end(),
        [](const QJsonObject& status) {
            return status.value(QStringLiteral("status")).toString() == QStringLiteral("running") &&
                   !status.value(QStringLiteral("review")).toObject()
                        .value(QStringLiteral("entries")).toArray().isEmpty();
        });
    REQUIRE(running_with_preview != statuses.end());

    const QJsonObject plan = read_status(Utils::utf8_to_path(review_file.toStdString()));
    CHECK(plan.value(QStringLiteral("kind")).toString() == QStringLiteral("aifs.headlessReviewPlan"));
    CHECK(plan.value(QStringLiteral("operation")).toString() == QStringLiteral("categorize"));
    CHECK(plan.value(QStringLiteral("entries")).toArray().size() == 1);
}

TEST_CASE("HeadlessAnalysisCommand preserves UTF-8 filenames in review status")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QTemporaryDir config_root;
    REQUIRE(config_root.isValid());
    ScopedEnvironmentVariable config_env("AI_FILE_SORTER_CONFIG_DIR",
                                         config_root.path().toUtf8());

    const std::filesystem::path target =
        std::filesystem::path(dir.filePath(QStringLiteral("target")).toStdString());
    const std::string target_text = Utils::path_to_utf8(target);
    REQUIRE(QDir().mkpath(QString::fromUtf8(target_text.data(),
                                            static_cast<qsizetype>(target_text.size()))));
    const std::string file_name = utf8_review_filename();
    const std::filesystem::path source = make_file_at(target / Utils::utf8_to_path(file_name));

    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    cache_categorization(db, target, file_name, "Documents", "Reports");

    const std::filesystem::path runtime_dir = std::filesystem::path(dir.path().toStdString()) / "runtime";
    const std::filesystem::path status = std::filesystem::path(dir.path().toStdString()) / "status.json";

    HeadlessAnalysisCommand::Options options;
    options.operation = HeadlessAnalysisCommand::Operation::Categorize;
    options.apply_mode = HeadlessAnalysisCommand::ApplyMode::ReviewOnly;
    options.paths.push_back(target);
    options.status_file = status;
    options.job_id = "headless-unicode-review";

    std::ostringstream out;
    std::ostringstream err;
    const int exit_code = HeadlessAnalysisCommand::run(options, runtime_dir, out, err);

    CHECK(exit_code == HeadlessAnalysisCommand::Success);
    const std::string source_text = Utils::path_to_utf8(source);
    CHECK(QFile::exists(QString::fromUtf8(source_text.data(),
                                          static_cast<qsizetype>(source_text.size()))));
    const QJsonObject object = read_status(status);
    CHECK(object.value(QStringLiteral("status")).toString() == QStringLiteral("review_required"));

    const QJsonArray entries = object.value(QStringLiteral("review")).toObject()
                                   .value(QStringLiteral("entries")).toArray();
    REQUIRE(entries.size() == 1);
    const QJsonObject entry = entries.at(0).toObject();
    const QString expected_name =
        QString::fromUtf8(file_name.data(), static_cast<qsizetype>(file_name.size()));
    const QString actual_name = entry.value(QStringLiteral("fileName")).toString();
    CHECK(actual_name == expected_name);
    CHECK_FALSE(actual_name.contains(QChar::ReplacementCharacter));
    CHECK(entry.value(QStringLiteral("source")).toString().contains(expected_name));
    CHECK(entry.value(QStringLiteral("destination")).toString().contains(expected_name));
    CHECK_FALSE(entry.value(QStringLiteral("destination")).toString().contains(QChar::ReplacementCharacter));
}

TEST_CASE("HeadlessAnalysisCommand applies saved review plan")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QTemporaryDir config_root;
    REQUIRE(config_root.isValid());
    ScopedEnvironmentVariable config_env("AI_FILE_SORTER_CONFIG_DIR",
                                         config_root.path().toUtf8());

    const std::filesystem::path target =
        std::filesystem::path(dir.filePath(QStringLiteral("target")).toStdString());
    REQUIRE(QDir().mkpath(QString::fromStdString(Utils::path_to_utf8(target))));
    const std::filesystem::path source = make_file_at(target / "input.txt");
    const std::filesystem::path destination = target / "Documents" / "Reports" / "input.txt";

    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    cache_categorization(db, target, "input.txt", "Documents", "Reports");

    const std::filesystem::path runtime_dir = std::filesystem::path(dir.path().toStdString()) / "runtime";
    const std::filesystem::path status = std::filesystem::path(dir.path().toStdString()) / "status.json";

    HeadlessAnalysisCommand::Options options;
    options.operation = HeadlessAnalysisCommand::Operation::Categorize;
    options.paths.push_back(target);
    options.status_file = status;
    options.job_id = "headless-review-plan";

    std::ostringstream out;
    std::ostringstream err;
    REQUIRE(HeadlessAnalysisCommand::run(options, runtime_dir, out, err) ==
            HeadlessAnalysisCommand::Success);
    CHECK(QFile::exists(QString::fromStdString(Utils::path_to_utf8(source))));

    const QJsonObject review_status = read_status(status);
    const QString review_file_text = review_status.value(QStringLiteral("reviewFile")).toString();
    REQUIRE_FALSE(review_file_text.isEmpty());
    const std::filesystem::path review_file = Utils::utf8_to_path(review_file_text.toStdString());

    HeadlessAnalysisCommand::Options apply_options;
    apply_options.request_mode = HeadlessAnalysisCommand::RequestMode::ApplyReview;
    apply_options.review_file = review_file;
    apply_options.status_file = status;
    apply_options.job_id = "headless-review-plan-apply";

    std::ostringstream apply_out;
    std::ostringstream apply_err;
    const int apply_exit =
        HeadlessAnalysisCommand::run(apply_options, runtime_dir, apply_out, apply_err);

    CHECK(apply_exit == HeadlessAnalysisCommand::Success);
    CHECK_FALSE(QFile::exists(QString::fromStdString(Utils::path_to_utf8(source))));
    CHECK(QFile::exists(QString::fromStdString(Utils::path_to_utf8(destination))));

    const QJsonObject object = read_status(status);
    CHECK(object.value(QStringLiteral("status")).toString() == QStringLiteral("completed"));
    CHECK(object.value(QStringLiteral("operation")).toString() == QStringLiteral("categorize"));
    CHECK(object.value(QStringLiteral("reviewFile")).toString() == review_file_text);
    const QJsonObject apply = object.value(QStringLiteral("apply")).toObject();
    CHECK(apply.value(QStringLiteral("movedCount")).toInt() == 1);
    CHECK(apply.value(QStringLiteral("renamedCount")).toInt() == 0);
    CHECK(apply.value(QStringLiteral("skippedCount")).toInt() == 0);
    CHECK(apply.value(QStringLiteral("undoPlanSaved")).toBool());
}

TEST_CASE("HeadlessAnalysisCommand categorizes only a selected file target")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QTemporaryDir config_root;
    REQUIRE(config_root.isValid());
    ScopedEnvironmentVariable config_env("AI_FILE_SORTER_CONFIG_DIR",
                                         config_root.path().toUtf8());

    const std::filesystem::path target =
        std::filesystem::path(dir.filePath(QStringLiteral("target")).toStdString());
    REQUIRE(QDir().mkpath(QString::fromStdString(Utils::path_to_utf8(target))));
    const std::filesystem::path selected_source = make_file_at(target / "selected.txt");
    const std::filesystem::path sibling_source = make_file_at(target / "sibling.txt");
    const std::filesystem::path selected_destination =
        target / "Documents" / "Reports" / "selected.txt";
    const std::filesystem::path sibling_destination =
        target / "Documents" / "Reports" / "sibling.txt";

    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    cache_categorization(db, target, "selected.txt", "Documents", "Reports");

    const std::filesystem::path runtime_dir = std::filesystem::path(dir.path().toStdString()) / "runtime";
    const std::filesystem::path status = std::filesystem::path(dir.path().toStdString()) / "status.json";

    HeadlessAnalysisCommand::Options options;
    options.operation = HeadlessAnalysisCommand::Operation::Categorize;
    options.paths.push_back(selected_source);
    options.status_file = status;
    options.job_id = "headless-selected-file";
    enable_auto_apply(options);

    std::ostringstream out;
    std::ostringstream err;
    const int exit_code = HeadlessAnalysisCommand::run(options, runtime_dir, out, err);

    CHECK(exit_code == HeadlessAnalysisCommand::Success);
    CHECK_FALSE(QFile::exists(QString::fromStdString(Utils::path_to_utf8(selected_source))));
    CHECK(QFile::exists(QString::fromStdString(Utils::path_to_utf8(selected_destination))));
    CHECK(QFile::exists(QString::fromStdString(Utils::path_to_utf8(sibling_source))));
    CHECK_FALSE(QFile::exists(QString::fromStdString(Utils::path_to_utf8(sibling_destination))));

    const QJsonObject object = read_status(status);
    const QJsonObject review = object.value(QStringLiteral("review")).toObject();
    const QJsonObject apply = object.value(QStringLiteral("apply")).toObject();
    CHECK(review.value(QStringLiteral("entryCount")).toInt() == 1);
    CHECK(apply.value(QStringLiteral("movedCount")).toInt() == 1);
    CHECK(apply.value(QStringLiteral("skippedCount")).toInt() == 0);
}

TEST_CASE("HeadlessAnalysisCommand applies cached rename for a folder")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QTemporaryDir config_root;
    REQUIRE(config_root.isValid());
    ScopedEnvironmentVariable config_env("AI_FILE_SORTER_CONFIG_DIR",
                                         config_root.path().toUtf8());

    const std::filesystem::path target =
        std::filesystem::path(dir.filePath(QStringLiteral("target")).toStdString());
    REQUIRE(QDir().mkpath(QString::fromStdString(Utils::path_to_utf8(target))));
    const std::filesystem::path source = make_file_at(target / "report.txt");
    const std::filesystem::path destination = target / "renamed_report.txt";

    Settings settings;
    settings.set_offer_rename_documents(true);
    REQUIRE(settings.save());
    DatabaseManager db(settings.get_config_dir());
    const auto resolved = db.resolve_category("Documents", "Reports");
    REQUIRE(db.insert_or_update_file_with_categorization("report.txt",
                                                         "F",
                                                         normalized_path_key(target),
                                                         resolved,
                                                         false,
                                                         "renamed_report.txt"));

    const std::filesystem::path runtime_dir = std::filesystem::path(dir.path().toStdString()) / "runtime";
    const std::filesystem::path status = std::filesystem::path(dir.path().toStdString()) / "status.json";

    HeadlessAnalysisCommand::Options options;
    options.operation = HeadlessAnalysisCommand::Operation::Rename;
    options.paths.push_back(target);
    options.status_file = status;
    options.job_id = "headless-cached-rename";
    enable_auto_apply(options);

    std::ostringstream out;
    std::ostringstream err;
    const int exit_code = HeadlessAnalysisCommand::run(options, runtime_dir, out, err);

    CHECK(exit_code == HeadlessAnalysisCommand::Success);
    CHECK_FALSE(QFile::exists(QString::fromStdString(Utils::path_to_utf8(source))));
    CHECK(QFile::exists(QString::fromStdString(Utils::path_to_utf8(destination))));
    CHECK_FALSE(QFile::exists(QString::fromStdString(
        Utils::path_to_utf8(target / "Documents" / "Reports" / "renamed_report.txt"))));

    const QJsonObject object = read_status(status);
    CHECK(object.value(QStringLiteral("status")).toString() == QStringLiteral("completed"));
    CHECK(object.value(QStringLiteral("operation")).toString() == QStringLiteral("rename"));
    const QJsonObject apply = object.value(QStringLiteral("apply")).toObject();
    CHECK(apply.value(QStringLiteral("movedCount")).toInt() == 0);
    CHECK(apply.value(QStringLiteral("renamedCount")).toInt() == 1);
    CHECK(apply.value(QStringLiteral("skippedCount")).toInt() == 0);
    CHECK(apply.value(QStringLiteral("undoPlanSaved")).toBool());
}

TEST_CASE("HeadlessAnalysisCommand renames only a selected file target")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QTemporaryDir config_root;
    REQUIRE(config_root.isValid());
    ScopedEnvironmentVariable config_env("AI_FILE_SORTER_CONFIG_DIR",
                                         config_root.path().toUtf8());

    const std::filesystem::path target =
        std::filesystem::path(dir.filePath(QStringLiteral("target")).toStdString());
    REQUIRE(QDir().mkpath(QString::fromStdString(Utils::path_to_utf8(target))));
    const std::filesystem::path selected_source = make_file_at(target / "report.txt");
    const std::filesystem::path sibling_source = make_file_at(target / "notes.txt");
    const std::filesystem::path selected_destination = target / "renamed_report.txt";
    const std::filesystem::path sibling_destination = target / "renamed_notes.txt";

    Settings settings;
    settings.set_offer_rename_documents(true);
    REQUIRE(settings.save());
    DatabaseManager db(settings.get_config_dir());
    cache_categorization(db, target, "report.txt", "Documents", "Reports", "renamed_report.txt");

    const std::filesystem::path runtime_dir = std::filesystem::path(dir.path().toStdString()) / "runtime";
    const std::filesystem::path status = std::filesystem::path(dir.path().toStdString()) / "status.json";

    HeadlessAnalysisCommand::Options options;
    options.operation = HeadlessAnalysisCommand::Operation::Rename;
    options.paths.push_back(selected_source);
    options.status_file = status;
    options.job_id = "headless-selected-rename";
    enable_auto_apply(options);

    std::ostringstream out;
    std::ostringstream err;
    const int exit_code = HeadlessAnalysisCommand::run(options, runtime_dir, out, err);

    CHECK(exit_code == HeadlessAnalysisCommand::Success);
    CHECK_FALSE(QFile::exists(QString::fromStdString(Utils::path_to_utf8(selected_source))));
    CHECK(QFile::exists(QString::fromStdString(Utils::path_to_utf8(selected_destination))));
    CHECK(QFile::exists(QString::fromStdString(Utils::path_to_utf8(sibling_source))));
    CHECK_FALSE(QFile::exists(QString::fromStdString(Utils::path_to_utf8(sibling_destination))));

    const QJsonObject object = read_status(status);
    const QJsonObject apply = object.value(QStringLiteral("apply")).toObject();
    CHECK(apply.value(QStringLiteral("movedCount")).toInt() == 0);
    CHECK(apply.value(QStringLiteral("renamedCount")).toInt() == 1);
    CHECK(apply.value(QStringLiteral("skippedCount")).toInt() == 0);
}

TEST_CASE("HeadlessAnalysisCommand skips rename when no cached suggestion exists")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QTemporaryDir config_root;
    REQUIRE(config_root.isValid());
    ScopedEnvironmentVariable config_env("AI_FILE_SORTER_CONFIG_DIR",
                                         config_root.path().toUtf8());

    const std::filesystem::path target =
        std::filesystem::path(dir.filePath(QStringLiteral("target")).toStdString());
    REQUIRE(QDir().mkpath(QString::fromStdString(Utils::path_to_utf8(target))));
    const std::filesystem::path source = make_file_at(target / "archive.bin");

    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    const auto resolved = db.resolve_category("Archives", "Binary");
    REQUIRE(db.insert_or_update_file_with_categorization("archive.bin",
                                                         "F",
                                                         normalized_path_key(target),
                                                         resolved,
                                                         false));

    const std::filesystem::path runtime_dir = std::filesystem::path(dir.path().toStdString()) / "runtime";
    const std::filesystem::path status = std::filesystem::path(dir.path().toStdString()) / "status.json";

    HeadlessAnalysisCommand::Options options;
    options.operation = HeadlessAnalysisCommand::Operation::Rename;
    options.paths.push_back(target);
    options.status_file = status;
    options.job_id = "headless-rename-skip";

    std::ostringstream out;
    std::ostringstream err;
    const int exit_code = HeadlessAnalysisCommand::run(options, runtime_dir, out, err);

    CHECK(exit_code == HeadlessAnalysisCommand::Success);
    CHECK(QFile::exists(QString::fromStdString(Utils::path_to_utf8(source))));

    const QJsonObject object = read_status(status);
    const QJsonObject apply = object.value(QStringLiteral("apply")).toObject();
    CHECK(apply.value(QStringLiteral("movedCount")).toInt() == 0);
    CHECK(apply.value(QStringLiteral("renamedCount")).toInt() == 0);
    CHECK(apply.value(QStringLiteral("skippedCount")).toInt() == 1);
}

TEST_CASE("HeadlessAnalysisCommand applies cached categorize and rename for a folder")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QTemporaryDir config_root;
    REQUIRE(config_root.isValid());
    ScopedEnvironmentVariable config_env("AI_FILE_SORTER_CONFIG_DIR",
                                         config_root.path().toUtf8());

    const std::filesystem::path target =
        std::filesystem::path(dir.filePath(QStringLiteral("target")).toStdString());
    REQUIRE(QDir().mkpath(QString::fromStdString(Utils::path_to_utf8(target))));
    const std::filesystem::path source = make_file_at(target / "summary.txt");
    const std::filesystem::path destination =
        target / "Documents" / "Reports" / "renamed_summary.txt";

    Settings settings;
    settings.set_offer_rename_documents(true);
    REQUIRE(settings.save());
    DatabaseManager db(settings.get_config_dir());
    const auto resolved = db.resolve_category("Documents", "Reports");
    REQUIRE(db.insert_or_update_file_with_categorization("summary.txt",
                                                         "F",
                                                         normalized_path_key(target),
                                                         resolved,
                                                         false,
                                                         "renamed_summary.txt"));

    const std::filesystem::path runtime_dir = std::filesystem::path(dir.path().toStdString()) / "runtime";
    const std::filesystem::path status = std::filesystem::path(dir.path().toStdString()) / "status.json";

    HeadlessAnalysisCommand::Options options;
    options.operation = HeadlessAnalysisCommand::Operation::CategorizeAndRename;
    options.paths.push_back(target);
    options.status_file = status;
    options.job_id = "headless-cached-categorize-rename";
    enable_auto_apply(options);

    std::ostringstream out;
    std::ostringstream err;
    const int exit_code = HeadlessAnalysisCommand::run(options, runtime_dir, out, err);

    CHECK(exit_code == HeadlessAnalysisCommand::Success);
    CHECK_FALSE(QFile::exists(QString::fromStdString(Utils::path_to_utf8(source))));
    CHECK(QFile::exists(QString::fromStdString(Utils::path_to_utf8(destination))));

    const QJsonObject object = read_status(status);
    CHECK(object.value(QStringLiteral("status")).toString() == QStringLiteral("completed"));
    CHECK(object.value(QStringLiteral("operation")).toString() == QStringLiteral("categorize-and-rename"));
    const QJsonObject apply = object.value(QStringLiteral("apply")).toObject();
    CHECK(apply.value(QStringLiteral("movedCount")).toInt() == 1);
    CHECK(apply.value(QStringLiteral("renamedCount")).toInt() == 1);
    CHECK(apply.value(QStringLiteral("skippedCount")).toInt() == 0);
    CHECK(apply.value(QStringLiteral("undoPlanSaved")).toBool());
}

TEST_CASE("HeadlessAnalysisCommand applies same-folder multi-select only")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QTemporaryDir config_root;
    REQUIRE(config_root.isValid());
    ScopedEnvironmentVariable config_env("AI_FILE_SORTER_CONFIG_DIR",
                                         config_root.path().toUtf8());

    const std::filesystem::path target =
        std::filesystem::path(dir.filePath(QStringLiteral("target")).toStdString());
    REQUIRE(QDir().mkpath(QString::fromStdString(Utils::path_to_utf8(target))));
    const std::filesystem::path first_source = make_file_at(target / "first.txt");
    const std::filesystem::path second_source = make_file_at(target / "second.txt");
    const std::filesystem::path third_source = make_file_at(target / "third.txt");
    const std::filesystem::path first_destination =
        target / "Documents" / "Reports" / "first.txt";
    const std::filesystem::path second_destination =
        target / "Documents" / "Reports" / "second.txt";
    const std::filesystem::path third_destination =
        target / "Documents" / "Reports" / "third.txt";

    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    cache_categorization(db, target, "first.txt", "Documents", "Reports");
    cache_categorization(db, target, "second.txt", "Documents", "Reports");

    const std::filesystem::path runtime_dir = std::filesystem::path(dir.path().toStdString()) / "runtime";
    const std::filesystem::path status = std::filesystem::path(dir.path().toStdString()) / "status.json";

    HeadlessAnalysisCommand::Options options;
    options.operation = HeadlessAnalysisCommand::Operation::Categorize;
    options.paths.push_back(first_source);
    options.paths.push_back(second_source);
    options.status_file = status;
    options.job_id = "headless-multi-select";
    enable_auto_apply(options);

    std::ostringstream out;
    std::ostringstream err;
    const int exit_code = HeadlessAnalysisCommand::run(options, runtime_dir, out, err);

    CHECK(exit_code == HeadlessAnalysisCommand::Success);
    CHECK_FALSE(QFile::exists(QString::fromStdString(Utils::path_to_utf8(first_source))));
    CHECK_FALSE(QFile::exists(QString::fromStdString(Utils::path_to_utf8(second_source))));
    CHECK(QFile::exists(QString::fromStdString(Utils::path_to_utf8(first_destination))));
    CHECK(QFile::exists(QString::fromStdString(Utils::path_to_utf8(second_destination))));
    CHECK(QFile::exists(QString::fromStdString(Utils::path_to_utf8(third_source))));
    CHECK_FALSE(QFile::exists(QString::fromStdString(Utils::path_to_utf8(third_destination))));

    const QJsonObject object = read_status(status);
    const QJsonObject review = object.value(QStringLiteral("review")).toObject();
    const QJsonObject apply = object.value(QStringLiteral("apply")).toObject();
    CHECK(review.value(QStringLiteral("entryCount")).toInt() == 2);
    CHECK(apply.value(QStringLiteral("movedCount")).toInt() == 2);
    CHECK(apply.value(QStringLiteral("skippedCount")).toInt() == 0);
}

TEST_CASE("HeadlessReviewApplyService uses display folders and canonical storage")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QTemporaryDir config_root;
    REQUIRE(config_root.isValid());
    ScopedEnvironmentVariable config_env("AI_FILE_SORTER_CONFIG_DIR",
                                         config_root.path().toUtf8());

    const std::filesystem::path target =
        std::filesystem::path(dir.filePath(QStringLiteral("target")).toStdString());
    REQUIRE(QDir().mkpath(QString::fromStdString(Utils::path_to_utf8(target))));
    const std::filesystem::path source = make_file_at(target / "statement.txt");
    const std::filesystem::path destination =
        target / "Records_2026-06" / "Monthly Statements" / "statement.txt";

    CategorizedFile entry;
    entry.file_path = Utils::path_to_utf8(target);
    entry.file_name = "statement.txt";
    entry.type = FileType::File;
    entry.category = "Records_2026-06";
    entry.subcategory = "Monthly Statements";
    entry.canonical_category = "Records";
    entry.canonical_subcategory = "Monthly Statements";

    Settings settings;
    DatabaseManager db(settings.get_config_dir());
    LocalFsProvider storage_provider;
    HeadlessReviewApplyService service(&db, storage_provider, nullptr);

    HeadlessReviewApplyService::Options options;
    options.base_dir = Utils::path_to_utf8(std::filesystem::absolute(target).lexically_normal());
    options.undo_dir = Utils::path_to_utf8(std::filesystem::path(dir.path().toStdString()) / "undo");
    options.use_subcategories = true;
    options.include_subdirectories = true;

    const auto result = service.apply({entry}, options);

    CHECK(result.planned_count == 1);
    CHECK(result.moved_count == 1);
    CHECK(result.skipped_count == 0);
    CHECK(result.undo_plan_saved);
    REQUIRE(result.entries.size() == 1);
    CHECK_FALSE(QFile::exists(QString::fromStdString(Utils::path_to_utf8(source))));
    CHECK(QFile::exists(QString::fromStdString(Utils::path_to_utf8(destination))));

    const std::string destination_dir =
        Utils::path_to_utf8(Utils::utf8_to_path(result.entries.at(0).destination).parent_path());
    const auto cached = db.get_categorized_file(destination_dir, entry.file_name, FileType::File);
    REQUIRE(cached.has_value());
    CHECK(cached->category == "Records");
    CHECK(cached->subcategory == "Monthly Statements");
}

TEST_CASE("HeadlessAnalysisCommand rejects cross-folder file selections")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString first_dir = dir.filePath(QStringLiteral("first"));
    const QString second_dir = dir.filePath(QStringLiteral("second"));
    REQUIRE(QDir().mkpath(first_dir));
    REQUIRE(QDir().mkpath(second_dir));
    const std::filesystem::path first = make_file_at(
        std::filesystem::path(first_dir.toStdString()) / "first.txt");
    const std::filesystem::path second = make_file_at(
        std::filesystem::path(second_dir.toStdString()) / "second.txt");
    const std::filesystem::path runtime_dir = std::filesystem::path(dir.path().toStdString()) / "runtime";
    const std::filesystem::path status = std::filesystem::path(dir.path().toStdString()) / "status.json";

    HeadlessAnalysisCommand::Options options;
    options.operation = HeadlessAnalysisCommand::Operation::Rename;
    options.paths.push_back(first);
    options.paths.push_back(second);
    options.status_file = status;
    options.job_id = "headless-job";

    std::ostringstream out;
    std::ostringstream err;
    const int exit_code = HeadlessAnalysisCommand::run(options, runtime_dir, out, err);

    CHECK(exit_code == HeadlessAnalysisCommand::Unsupported);
    const QJsonObject object = read_status(status);
    CHECK(object.value(QStringLiteral("status")).toString() == QStringLiteral("failed"));
    CHECK(object.value(QStringLiteral("operation")).toString() == QStringLiteral("rename"));
    CHECK(object.value(QStringLiteral("error")).toString().contains(QStringLiteral("same-folder")));

    AnalysisRuntimeLock lock(runtime_dir);
    CHECK_FALSE(lock.is_locked());
}
