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

#include <sstream>

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

std::filesystem::path make_file_at(const std::filesystem::path& path)
{
    QFile file(QString::fromStdString(Utils::path_to_utf8(path)));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    REQUIRE(file.write("sample") == 6);
    file.close();
    return path;
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
    CHECK(parsed.options.operation == HeadlessAnalysisCommand::Operation::CategorizeAndRename);
    REQUIRE(parsed.options.paths.size() == 1);
    CHECK(parsed.options.paths.front() == target);
    REQUIRE(parsed.options.status_file.has_value());
    CHECK(*parsed.options.status_file == status);
    CHECK(parsed.options.job_id == "test-job");
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
    DatabaseManager db(settings.get_config_dir());
    const std::string target_key =
        Utils::path_to_utf8(std::filesystem::absolute(target).lexically_normal());
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

TEST_CASE("HeadlessAnalysisCommand releases runtime lock after unsupported execution")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const std::filesystem::path target = make_file(dir, QStringLiteral("input.txt"));
    const std::filesystem::path runtime_dir = std::filesystem::path(dir.path().toStdString()) / "runtime";
    const std::filesystem::path status = std::filesystem::path(dir.path().toStdString()) / "status.json";

    HeadlessAnalysisCommand::Options options;
    options.operation = HeadlessAnalysisCommand::Operation::Rename;
    options.paths.push_back(target);
    options.status_file = status;
    options.job_id = "headless-job";

    std::ostringstream out;
    std::ostringstream err;
    const int exit_code = HeadlessAnalysisCommand::run(options, runtime_dir, out, err);

    CHECK(exit_code == HeadlessAnalysisCommand::Unsupported);
    const QJsonObject object = read_status(status);
    CHECK(object.value(QStringLiteral("status")).toString() == QStringLiteral("failed"));
    CHECK(object.value(QStringLiteral("operation")).toString() == QStringLiteral("rename"));

    AnalysisRuntimeLock lock(runtime_dir);
    CHECK_FALSE(lock.is_locked());
}
