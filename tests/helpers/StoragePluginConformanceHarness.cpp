#include "StoragePluginConformanceHarness.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStringList>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <utility>

namespace {

constexpr auto kProtocol = "aifs-storage-plugin-v1";
constexpr int kOptionFiles = 1;
constexpr int kOptionDirectories = 2;
constexpr int kOptionHiddenFiles = 4;

struct InvocationResult {
    bool launched{false};
    QJsonObject response;
    QByteArray stdout_data;
    QByteArray stderr_data;
    QString process_error;
};

struct FixturePaths {
    std::filesystem::path root;
    std::filesystem::path source_file;
    std::filesystem::path nested_dir;
    std::filesystem::path nested_file;
    std::filesystem::path hidden_file;
    std::filesystem::path locked_file;
    std::filesystem::path missing_file;
    std::filesystem::path ensured_dir;
    std::filesystem::path moved_file;
    std::filesystem::path move_created_dir;
    std::filesystem::path conflicting_destination;
};

std::string make_unique_suffix() {
    const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    return std::to_string(now);
}

std::string to_string(const QString& value) {
    return value.toStdString();
}

QString to_qstring(const std::filesystem::path& path) {
    return QString::fromStdString(path.string());
}

std::string json_compact(const QJsonObject& object) {
    return QJsonDocument(object).toJson(QJsonDocument::Compact).toStdString();
}

void write_file(const std::filesystem::path& path, std::string_view payload) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
}

std::filesystem::path absolute_path(const std::filesystem::path& path) {
    std::error_code ec;
    const auto resolved = std::filesystem::absolute(path, ec);
    return ec ? path : resolved;
}

std::filesystem::path default_fixture_parent() {
    std::error_code ec;
    const auto temp = std::filesystem::temp_directory_path(ec);
    return ec ? std::filesystem::current_path() : temp;
}

FixturePaths create_fixture_tree(const StoragePluginConformanceOptions& options) {
    const auto parent = options.fixture_parent.empty() ? default_fixture_parent() : options.fixture_parent;
    FixturePaths paths;
    paths.root = parent / ("AIFS-MockCloud-Conformance-" + make_unique_suffix());
    paths.source_file = paths.root / "source.txt";
    paths.nested_dir = paths.root / "Nested";
    paths.nested_file = paths.nested_dir / "nested.txt";
    paths.hidden_file = paths.root / ".hidden.txt";
    paths.locked_file = paths.root / "~$locked.docx";
    paths.missing_file = paths.root / "missing.txt";
    paths.ensured_dir = paths.root / "Ensured" / "Leaf";
    paths.move_created_dir = paths.root / "MovedByConnector";
    paths.moved_file = paths.move_created_dir / "source.txt";
    paths.conflicting_destination = paths.root / "existing-destination.txt";

    write_file(paths.source_file, "source");
    write_file(paths.nested_file, "nested");
    write_file(paths.hidden_file, "hidden");
    write_file(paths.locked_file, "locked");
    write_file(paths.conflicting_destination, "existing");
    return paths;
}

QJsonObject make_request(const StoragePluginConformanceOptions& options, std::string_view action) {
    QJsonObject request;
    request["protocol"] = QString::fromLatin1(kProtocol);
    request["plugin_id"] = QString::fromStdString(options.plugin_id);
    request["provider_id"] = action == "probe" ? QString() : QString::fromStdString(options.provider_id);
    request["action"] = QString::fromUtf8(action.data(), static_cast<qsizetype>(action.size()));
    return request;
}

void prepend_path_entry(QProcessEnvironment* environment, const QString& directory) {
    if (!environment || directory.isEmpty()) {
        return;
    }

#ifdef _WIN32
    const QString path_key =
        environment->contains(QStringLiteral("Path")) ? QStringLiteral("Path") : QStringLiteral("PATH");
    const QString separator = QStringLiteral(";");
#else
    const QString path_key = QStringLiteral("PATH");
    const QString separator = QStringLiteral(":");
#endif

    const QString existing = environment->value(path_key);
    const QStringList entries = existing.split(separator, Qt::SkipEmptyParts);
    if (!entries.contains(directory, Qt::CaseInsensitive)) {
        environment->insert(path_key, directory + (existing.isEmpty() ? QString() : separator + existing));
    }
}

QProcessEnvironment connector_environment(const std::filesystem::path& connector) {
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    prepend_path_entry(&environment, QString::fromStdString(connector.parent_path().string()));
    prepend_path_entry(&environment, QCoreApplication::applicationDirPath());
    return environment;
}

InvocationResult invoke_connector(const StoragePluginConformanceOptions& options, const QJsonObject& request,
                                  std::vector<std::string>* failures) {
    InvocationResult result;
    const std::filesystem::path connector = absolute_path(options.connector_executable);
    if (connector.empty()) {
        if (failures) {
            failures->push_back("connector executable path is empty");
        }
        return result;
    }

    QProcess process;
    process.setProcessEnvironment(connector_environment(connector));
    if (!connector.parent_path().empty()) {
        process.setWorkingDirectory(QString::fromStdString(connector.parent_path().string()));
    }

    process.start(QString::fromStdString(connector.string()), {});
    if (!process.waitForStarted(options.timeout_ms)) {
        result.process_error = process.errorString();
        if (failures) {
            failures->push_back("failed to start connector '" + connector.string() +
                                "': " + to_string(result.process_error));
        }
        return result;
    }
    result.launched = true;

    const QByteArray payload = QJsonDocument(request).toJson(QJsonDocument::Compact);
    process.write(payload);
    if (!process.waitForBytesWritten(options.timeout_ms)) {
        result.process_error = process.errorString();
        if (failures) {
            failures->push_back("failed to write request " + json_compact(request) +
                                " to connector: " + to_string(result.process_error));
        }
        process.kill();
        process.waitForFinished(1000);
        return result;
    }
    process.closeWriteChannel();

    if (!process.waitForFinished(options.timeout_ms)) {
        result.process_error = process.errorString();
        if (failures) {
            failures->push_back("connector timed out for request " + json_compact(request));
        }
        process.kill();
        process.waitForFinished(1000);
        return result;
    }

    if (process.exitStatus() != QProcess::NormalExit) {
        if (failures) {
            failures->push_back("connector crashed for request " + json_compact(request));
        }
    }

    result.stderr_data = process.readAllStandardError();
    result.stdout_data = process.readAllStandardOutput();

    QJsonParseError parse_error;
    const QJsonDocument doc = QJsonDocument::fromJson(result.stdout_data, &parse_error);
    if (!doc.isObject()) {
        if (failures) {
            const auto stderr_text = QString::fromUtf8(result.stderr_data).trimmed().toStdString();
            failures->push_back("connector returned invalid JSON for request " + json_compact(request) + ": " +
                                parse_error.errorString().toStdString() +
                                (stderr_text.empty() ? std::string() : " stderr: " + stderr_text));
        }
        return result;
    }

    result.response = doc.object();
    return result;
}

void add_failure(std::vector<std::string>* failures, std::string_view action, std::string_view message) {
    if (failures) {
        failures->push_back(std::string(action) + ": " + std::string(message));
    }
}

bool require_field(const QJsonObject& object, QStringView name, std::string_view action,
                   std::vector<std::string>* failures) {
    if (object.contains(name)) {
        return true;
    }
    add_failure(failures, action, "missing required field '" + name.toString().toStdString() + "'");
    return false;
}

bool require_bool(const QJsonObject& object, QStringView name, std::string_view action,
                  std::vector<std::string>* failures) {
    if (!require_field(object, name, action, failures)) {
        return false;
    }
    if (object.value(name).isBool()) {
        return true;
    }
    add_failure(failures, action, "field '" + name.toString().toStdString() + "' must be a boolean");
    return false;
}

bool require_string(const QJsonObject& object, QStringView name, std::string_view action,
                    std::vector<std::string>* failures) {
    if (!require_field(object, name, action, failures)) {
        return false;
    }
    if (object.value(name).isString()) {
        return true;
    }
    add_failure(failures, action, "field '" + name.toString().toStdString() + "' must be a string");
    return false;
}

bool require_integer(const QJsonObject& object, QStringView name, std::string_view action,
                     std::vector<std::string>* failures) {
    if (!require_field(object, name, action, failures)) {
        return false;
    }
    const QJsonValue value = object.value(name);
    const double number = value.toDouble();
    if (value.isDouble() && std::isfinite(number) && std::floor(number) == number) {
        return true;
    }
    add_failure(failures, action, "field '" + name.toString().toStdString() + "' must be an integer");
    return false;
}

bool require_object(const QJsonObject& object, QStringView name, std::string_view action, QJsonObject* nested,
                    std::vector<std::string>* failures) {
    if (!require_field(object, name, action, failures)) {
        return false;
    }
    if (!object.value(name).isObject()) {
        add_failure(failures, action, "field '" + name.toString().toStdString() + "' must be an object");
        return false;
    }
    if (nested) {
        *nested = object.value(name).toObject();
    }
    return true;
}

bool require_array(const QJsonObject& object, QStringView name, std::string_view action, QJsonArray* nested,
                   std::vector<std::string>* failures) {
    if (!require_field(object, name, action, failures)) {
        return false;
    }
    if (!object.value(name).isArray()) {
        add_failure(failures, action, "field '" + name.toString().toStdString() + "' must be an array");
        return false;
    }
    if (nested) {
        *nested = object.value(name).toArray();
    }
    return true;
}

bool validate_path_status(const QJsonObject& status, std::string_view action, std::vector<std::string>* failures) {
    bool ok = true;
    ok = require_bool(status, QStringLiteral("exists"), action, failures) && ok;
    ok = require_bool(status, QStringLiteral("hydration_required"), action, failures) && ok;
    ok = require_bool(status, QStringLiteral("sync_locked"), action, failures) && ok;
    ok = require_bool(status, QStringLiteral("conflict_copy"), action, failures) && ok;
    ok = require_bool(status, QStringLiteral("should_retry"), action, failures) && ok;
    ok = require_integer(status, QStringLiteral("retry_after_ms"), action, failures) && ok;
    ok = require_string(status, QStringLiteral("stable_identity"), action, failures) && ok;
    ok = require_string(status, QStringLiteral("revision_token"), action, failures) && ok;
    ok = require_string(status, QStringLiteral("message"), action, failures) && ok;
    return ok;
}

bool validate_preflight(const QJsonObject& preflight, std::string_view action, std::vector<std::string>* failures) {
    bool ok = true;
    ok = require_bool(preflight, QStringLiteral("allowed"), action, failures) && ok;
    ok = require_bool(preflight, QStringLiteral("skipped"), action, failures) && ok;
    ok = require_bool(preflight, QStringLiteral("hydration_required"), action, failures) && ok;
    ok = require_bool(preflight, QStringLiteral("sync_locked"), action, failures) && ok;
    ok = require_bool(preflight, QStringLiteral("destination_conflict"), action, failures) && ok;
    ok = require_bool(preflight, QStringLiteral("should_retry"), action, failures) && ok;
    ok = require_integer(preflight, QStringLiteral("retry_after_ms"), action, failures) && ok;
    ok = require_string(preflight, QStringLiteral("message"), action, failures) && ok;

    QJsonObject source_status;
    QJsonObject destination_status;
    if (require_object(preflight, QStringLiteral("source_status"), action, &source_status, failures)) {
        ok = validate_path_status(source_status, action, failures) && ok;
    } else {
        ok = false;
    }
    if (require_object(preflight, QStringLiteral("destination_status"), action, &destination_status, failures)) {
        ok = validate_path_status(destination_status, action, failures) && ok;
    } else {
        ok = false;
    }
    return ok;
}

bool validate_base_success(const QJsonObject& response, std::string_view action, std::vector<std::string>* failures) {
    bool ok = true;
    if (response.contains(QStringLiteral("protocol"))) {
        if (!response.value(QStringLiteral("protocol")).isString()) {
            add_failure(failures, action, "optional field 'protocol' must be a string");
            ok = false;
        } else if (response.value(QStringLiteral("protocol")).toString() != QString::fromLatin1(kProtocol)) {
            add_failure(failures, action, "optional field 'protocol' has unsupported value");
            ok = false;
        }
    }

    if (!require_bool(response, QStringLiteral("success"), action, failures)) {
        return false;
    }
    if (!response.value(QStringLiteral("success")).toBool(false)) {
        require_string(response, QStringLiteral("error"), action, failures);
        add_failure(failures, action, "fixture request returned success=false");
        return false;
    }
    return ok;
}

bool validate_response_for_action(const QJsonObject& response, std::string_view action,
                                  std::vector<std::string>* failures) {
    if (!validate_base_success(response, action, failures)) {
        return false;
    }

    bool ok = true;
    if (action == "probe") {
        QJsonArray provider_ids;
        ok = require_array(response, QStringLiteral("provider_ids"), action, &provider_ids, failures) && ok;
        if (provider_ids.isEmpty()) {
            add_failure(failures, action, "provider_ids must not be empty");
            ok = false;
        }
        for (const auto& value : provider_ids) {
            if (!value.isString() || value.toString().isEmpty()) {
                add_failure(failures, action, "provider_ids entries must be non-empty strings");
                ok = false;
            }
        }
        return ok;
    }

    if (action == "detect") {
        QJsonObject detection;
        if (!require_object(response, QStringLiteral("detection"), action, &detection, failures)) {
            return false;
        }
        ok = require_bool(detection, QStringLiteral("matched"), action, failures) && ok;
        if (detection.contains(QStringLiteral("confidence"))) {
            ok = require_integer(detection, QStringLiteral("confidence"), action, failures) && ok;
        }
        return ok;
    }

    if (action == "capabilities") {
        QJsonObject capabilities;
        if (!require_object(response, QStringLiteral("capabilities"), action, &capabilities, failures)) {
            return false;
        }
        ok = require_bool(capabilities, QStringLiteral("supports_online_only_files"), action, failures) && ok;
        ok = require_bool(capabilities, QStringLiteral("supports_atomic_rename"), action, failures) && ok;
        ok = require_bool(capabilities, QStringLiteral("should_skip_reparse_points"), action, failures) && ok;
        ok = require_bool(capabilities, QStringLiteral("should_relax_undo_mtime_validation"), action, failures) && ok;
        return ok;
    }

    if (action == "list_directory") {
        QJsonArray entries;
        ok = require_array(response, QStringLiteral("entries"), action, &entries, failures) && ok;
        for (const auto& value : entries) {
            if (!value.isObject()) {
                add_failure(failures, action, "entries values must be objects");
                ok = false;
                continue;
            }
            const QJsonObject entry = value.toObject();
            ok = require_string(entry, QStringLiteral("full_path"), action, failures) && ok;
            ok = require_string(entry, QStringLiteral("file_name"), action, failures) && ok;
            ok = require_string(entry, QStringLiteral("type"), action, failures) && ok;
            const QString type = entry.value(QStringLiteral("type")).toString();
            if (!type.isEmpty() && type != QStringLiteral("File") && type != QStringLiteral("Directory")) {
                add_failure(failures, action, "entry type must be File or Directory");
                ok = false;
            }
        }
        return ok;
    }

    if (action == "inspect_path") {
        QJsonObject status;
        return require_object(response, QStringLiteral("status"), action, &status, failures) &&
               validate_path_status(status, action, failures);
    }

    if (action == "preflight_move") {
        QJsonObject preflight;
        return require_object(response, QStringLiteral("preflight"), action, &preflight, failures) &&
               validate_preflight(preflight, action, failures);
    }

    if (action == "path_exists") {
        return require_bool(response, QStringLiteral("exists"), action, failures);
    }

    if (action == "ensure_directory") {
        return true;
    }

    if (action == "move_entry" || action == "undo_move") {
        ok = require_bool(response, QStringLiteral("mutation_success"), action, failures) && ok;
        ok = require_bool(response, QStringLiteral("skipped"), action, failures) && ok;
        ok = require_string(response, QStringLiteral("message"), action, failures) && ok;
        QJsonObject metadata;
        ok = require_object(response, QStringLiteral("metadata"), action, &metadata, failures) && ok;
        if (metadata.contains(QStringLiteral("size_bytes"))) {
            ok = require_integer(metadata, QStringLiteral("size_bytes"), action, failures) && ok;
        }
        if (metadata.contains(QStringLiteral("mtime"))) {
            ok = require_integer(metadata, QStringLiteral("mtime"), action, failures) && ok;
        }
        if (metadata.contains(QStringLiteral("stable_identity"))) {
            ok = require_string(metadata, QStringLiteral("stable_identity"), action, failures) && ok;
        }
        if (metadata.contains(QStringLiteral("revision_token"))) {
            ok = require_string(metadata, QStringLiteral("revision_token"), action, failures) && ok;
        }
        return ok;
    }

    add_failure(failures, action, "test harness does not recognize action");
    return false;
}

bool has_entry_named(const QJsonArray& entries, const QString& name) {
    for (const auto& value : entries) {
        if (value.isObject() && value.toObject().value(QStringLiteral("file_name")).toString() == name) {
            return true;
        }
    }
    return false;
}

void require_condition(bool condition, std::vector<std::string>* failures, std::string_view action,
                       std::string_view message) {
    if (!condition) {
        add_failure(failures, action, message);
    }
}

QJsonObject run_request(const StoragePluginConformanceOptions& options, std::string_view action, QJsonObject request,
                        std::vector<std::string>* failures) {
    const std::size_t failure_count = failures ? failures->size() : 0;
    const InvocationResult invocation = invoke_connector(options, request, failures);
    if (!invocation.launched || (failures && failures->size() > failure_count && invocation.response.isEmpty())) {
        return {};
    }
    validate_response_for_action(invocation.response, action, failures);
    return invocation.response;
}

}  // namespace

StoragePluginConformanceHarness::StoragePluginConformanceHarness(StoragePluginConformanceOptions options)
    : options_(std::move(options)) {}

StoragePluginConformanceResult StoragePluginConformanceHarness::run() {
    StoragePluginConformanceResult result;
    if (options_.connector_executable.empty()) {
        result.failures.push_back("connector executable path is required");
        return result;
    }

    const FixturePaths paths = create_fixture_tree(options_);
    result.fixture_root = paths.root;

    auto cleanup = [&]() {
        if (!options_.keep_fixture) {
            std::error_code ec;
            std::filesystem::remove_all(paths.root, ec);
        }
    };

    QJsonObject probe = make_request(options_, "probe");
    QJsonObject probe_response = run_request(options_, "probe", probe, &result.failures);
    const QJsonArray provider_ids = probe_response.value(QStringLiteral("provider_ids")).toArray();
    bool provider_exposed = false;
    for (const auto& value : provider_ids) {
        provider_exposed = provider_exposed || value.toString().toStdString() == options_.provider_id;
    }
    require_condition(provider_exposed, &result.failures, "probe",
                      "candidate did not expose the requested provider id");

    QJsonObject detect = make_request(options_, "detect");
    detect["root_path"] = to_qstring(paths.root);
    const QJsonObject detect_response = run_request(options_, "detect", detect, &result.failures);
    if (options_.require_detect_match) {
        const QJsonObject detection = detect_response.value(QStringLiteral("detection")).toObject();
        require_condition(detection.value(QStringLiteral("matched")).toBool(false), &result.failures, "detect",
                          "fixture root was not detected as supported by the provider");
    }

    QJsonObject capabilities = make_request(options_, "capabilities");
    run_request(options_, "capabilities", capabilities, &result.failures);

    QJsonObject list_directory = make_request(options_, "list_directory");
    list_directory["directory"] = to_qstring(paths.root);
    list_directory["options"] = kOptionFiles | kOptionDirectories | kOptionHiddenFiles;
    const QJsonObject list_response = run_request(options_, "list_directory", list_directory, &result.failures);
    const QJsonArray entries = list_response.value(QStringLiteral("entries")).toArray();
    require_condition(has_entry_named(entries, QStringLiteral("source.txt")), &result.failures, "list_directory",
                      "entries did not include source.txt");
    require_condition(has_entry_named(entries, QStringLiteral("Nested")), &result.failures, "list_directory",
                      "entries did not include the Nested directory");
    require_condition(has_entry_named(entries, QStringLiteral(".hidden.txt")), &result.failures, "list_directory",
                      "entries did not include hidden files when requested");

    QJsonObject inspect_path = make_request(options_, "inspect_path");
    inspect_path["path"] = to_qstring(paths.source_file);
    const QJsonObject inspect_response = run_request(options_, "inspect_path", inspect_path, &result.failures);
    const QJsonObject source_status = inspect_response.value(QStringLiteral("status")).toObject();
    require_condition(source_status.value(QStringLiteral("exists")).toBool(false), &result.failures, "inspect_path",
                      "source file should exist");

    QJsonObject preflight_move = make_request(options_, "preflight_move");
    preflight_move["source"] = to_qstring(paths.source_file);
    preflight_move["destination"] = to_qstring(paths.moved_file);
    const QJsonObject preflight_response = run_request(options_, "preflight_move", preflight_move, &result.failures);
    const QJsonObject preflight = preflight_response.value(QStringLiteral("preflight")).toObject();
    require_condition(preflight.value(QStringLiteral("allowed")).toBool(false), &result.failures, "preflight_move",
                      "new destination should be allowed");

    QJsonObject conflicting_preflight = make_request(options_, "preflight_move");
    conflicting_preflight["source"] = to_qstring(paths.source_file);
    conflicting_preflight["destination"] = to_qstring(paths.conflicting_destination);
    const QJsonObject conflicting_response =
        run_request(options_, "preflight_move", conflicting_preflight, &result.failures);
    const QJsonObject conflicting = conflicting_response.value(QStringLiteral("preflight")).toObject();
    require_condition(!conflicting.value(QStringLiteral("allowed")).toBool(true), &result.failures, "preflight_move",
                      "existing destination should not be allowed");
    require_condition(conflicting.value(QStringLiteral("destination_conflict")).toBool(false), &result.failures,
                      "preflight_move", "existing destination should be reported as a conflict");

    QJsonObject path_exists = make_request(options_, "path_exists");
    path_exists["path"] = to_qstring(paths.source_file);
    const QJsonObject path_exists_response = run_request(options_, "path_exists", path_exists, &result.failures);
    require_condition(path_exists_response.value(QStringLiteral("exists")).toBool(false), &result.failures,
                      "path_exists", "source file should exist");

    QJsonObject path_missing = make_request(options_, "path_exists");
    path_missing["path"] = to_qstring(paths.missing_file);
    const QJsonObject path_missing_response = run_request(options_, "path_exists", path_missing, &result.failures);
    require_condition(!path_missing_response.value(QStringLiteral("exists")).toBool(true), &result.failures,
                      "path_exists", "missing file should not exist");

    QJsonObject ensure_directory = make_request(options_, "ensure_directory");
    ensure_directory["directory"] = to_qstring(paths.ensured_dir);
    run_request(options_, "ensure_directory", ensure_directory, &result.failures);
    require_condition(std::filesystem::is_directory(paths.ensured_dir), &result.failures, "ensure_directory",
                      "connector did not create the requested directory");

    QJsonObject move_entry = make_request(options_, "move_entry");
    move_entry["source"] = to_qstring(paths.source_file);
    move_entry["destination"] = to_qstring(paths.moved_file);
    const QJsonObject move_response = run_request(options_, "move_entry", move_entry, &result.failures);
    require_condition(move_response.value(QStringLiteral("mutation_success")).toBool(false), &result.failures,
                      "move_entry", "move_entry should report mutation_success=true");
    require_condition(std::filesystem::exists(paths.moved_file), &result.failures, "move_entry",
                      "destination file does not exist after move_entry");
    require_condition(!std::filesystem::exists(paths.source_file), &result.failures, "move_entry",
                      "source file still exists after move_entry");

    QJsonObject undo_move = make_request(options_, "undo_move");
    undo_move["source"] = to_qstring(paths.source_file);
    undo_move["destination"] = to_qstring(paths.moved_file);
    QJsonArray created_directories;
    created_directories.push_back(to_qstring(paths.move_created_dir));
    undo_move["created_directories"] = created_directories;
    const QJsonObject undo_response = run_request(options_, "undo_move", undo_move, &result.failures);
    require_condition(undo_response.value(QStringLiteral("mutation_success")).toBool(false), &result.failures,
                      "undo_move", "undo_move should report mutation_success=true");
    require_condition(std::filesystem::exists(paths.source_file), &result.failures, "undo_move",
                      "source file was not restored by undo_move");
    require_condition(!std::filesystem::exists(paths.moved_file), &result.failures, "undo_move",
                      "moved file still exists after undo_move");

    cleanup();
    result.passed = result.failures.empty();
    return result;
}
