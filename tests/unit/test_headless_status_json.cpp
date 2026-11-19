#include <catch2/catch_test_macros.hpp>

#include "CategoryLanguage.hpp"
#include "HeadlessStatusJson.hpp"
#include "LocalFsProvider.hpp"
#include "Utils.hpp"

#include <QTemporaryDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::string utf8_probe_name()
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

std::string utf8_suggested_name()
{
    return std::string("energie_") +
           "\xE4\xBC\x9A\xE8\xAE\xAE" +
           "_" +
           "\xE0\xA4\xB0\xE0\xA4\xBF\xE0\xA4\xAA\xE0\xA5\x8B\xE0\xA4\xB0\xE0\xA5\x8D"
           "\xE0\xA4\x9F" +
           ".txt";
}

} // namespace

TEST_CASE("HeadlessStatusJson round-trips UTF-8 review plans")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    const std::filesystem::path root =
        std::filesystem::path(dir.filePath(QStringLiteral("target")).toStdString());
    const std::filesystem::path review_file =
        std::filesystem::path(dir.filePath(QStringLiteral("plans/review.json")).toStdString());

    const std::string file_name = utf8_probe_name();
    const std::filesystem::path source = root / Utils::utf8_to_path(file_name);
    const std::string suggested_name = utf8_suggested_name();

    CategorizedFile entry;
    entry.file_path = Utils::path_to_utf8(source);
    entry.file_name = file_name;
    entry.type = FileType::File;
    entry.category = "Documents";
    entry.subcategory = "Reports";
    entry.taxonomy_id = 42;
    entry.from_cache = true;
    entry.used_consistency_hints = true;
    entry.suggested_name = suggested_name;
    entry.rename_only = true;
    entry.canonical_category = "Documents";
    entry.canonical_subcategory = "Reports";
    entry.learning_context = "unicode review plan";
    entry.target_folder_relative_path = "10-19 Admin/11 Reports";
    entry.folder_tree_routing_explanation = "Reports for " + utf8_probe_name();
    entry.folder_tree_mode = true;
    entry.target_folder_suggested_new = true;
    entry.target_folder_exists = false;
    entry.folder_tree_allow_new_folders = true;

    HeadlessReviewApplyService::Options apply_options;
    apply_options.base_dir = Utils::path_to_utf8(root);
    apply_options.undo_dir = Utils::path_to_utf8(root / "undo");
    apply_options.use_subcategories = true;
    apply_options.include_subdirectories = true;
    apply_options.apply_suggested_names = true;
    apply_options.move_categorized_entries = false;
    apply_options.allow_new_folder_targets = true;
    apply_options.category_language = CategoryLanguage::French;

    HeadlessAnalysisCommand::Options options;
    options.operation = HeadlessAnalysisCommand::Operation::CategorizeAndRename;
    options.paths.push_back(root);
    options.job_id = "headless-status-json-round-trip";

    std::string error;
    REQUIRE(HeadlessStatusJson::write_review_plan_file(
        review_file,
        options,
        std::vector<CategorizedFile>{entry},
        apply_options,
        &error));

    HeadlessStatusJson::ReviewPlan plan;
    REQUIRE(HeadlessStatusJson::read_review_plan_file(review_file, &plan, &error));

    CHECK(plan.operation == HeadlessAnalysisCommand::Operation::CategorizeAndRename);
    REQUIRE(plan.paths.size() == 1);
    CHECK(Utils::path_to_utf8(plan.paths.front()) == Utils::path_to_utf8(root));
    CHECK(plan.apply_options.base_dir == apply_options.base_dir);
    CHECK(plan.apply_options.undo_dir == apply_options.undo_dir);
    CHECK(plan.apply_options.include_subdirectories);
    CHECK(plan.apply_options.apply_suggested_names);
    CHECK_FALSE(plan.apply_options.move_categorized_entries);
    CHECK(plan.apply_options.allow_new_folder_targets);
    CHECK(plan.apply_options.category_language == CategoryLanguage::French);

    REQUIRE(plan.entries.size() == 1);
    const CategorizedFile& loaded = plan.entries.front();
    CHECK(loaded.file_path == entry.file_path);
    CHECK(loaded.file_name == file_name);
    CHECK(loaded.suggested_name == suggested_name);
    CHECK(loaded.category == entry.category);
    CHECK(loaded.subcategory == entry.subcategory);
    CHECK(loaded.target_folder_relative_path == entry.target_folder_relative_path);
    CHECK(loaded.folder_tree_routing_explanation == entry.folder_tree_routing_explanation);
    CHECK(loaded.folder_tree_mode);
    CHECK(loaded.target_folder_suggested_new);
    CHECK_FALSE(loaded.target_folder_exists);
    CHECK(loaded.folder_tree_allow_new_folders);
    CHECK(loaded.rename_only);
}

TEST_CASE("HeadlessStatusJson exports authoritative review destinations and routing explanations", "[headless][review]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto root = Utils::utf8_to_path(dir.path().toStdString());
    const auto source = root / "inbox";
    const auto archive = root / "archive";
    std::filesystem::create_directories(source);
    std::filesystem::create_directories(archive / "Clients");
    std::vector<CategorizedFile> entries;
    for (const auto* name : {"one.txt", "two.txt"}) {
        std::ofstream(source / name) << "review metadata test";
        CategorizedFile entry;
        entry.file_path = Utils::path_to_utf8(source);
        entry.file_name = name;
        entry.type = FileType::File;
        entry.category = "Documents";
        entry.subcategory = "Reports";
        entry.suggested_name = "shared";
        entry.folder_tree_mode = true;
        entry.target_folder_relative_path = "Clients";
        entry.folder_tree_routing_explanation = "Client reports belong in Clients.";
        entries.push_back(entry);
    }
    HeadlessReviewApplyService::Options apply_options;
    apply_options.base_dir = Utils::path_to_utf8(archive);
    apply_options.apply_suggested_names = true;
    apply_options.apply_changes = false;
    SECTION("existing tree and duplicate suggestions") {}
    SECTION("generated categories") {
        for (auto& entry : entries) {
            entry.folder_tree_mode = false;
            entry.folder_tree_routing_explanation.clear();
        }
    }
    SECTION("new target folders") {
        apply_options.allow_new_folder_targets = true;
        for (auto& entry : entries) entry.target_folder_relative_path = "Pending/Reports";
    }
    SECTION("rename only ignores the archive root") {
        for (auto& entry : entries) entry.rename_only = true;
    }
    SECTION("unresolved targets have an empty planned destination") {
        entries[0].target_folder_relative_path.clear();
    }
    LocalFsProvider storage;
    HeadlessReviewApplyService service(nullptr, storage, nullptr);
    const auto preview = service.apply(entries, apply_options);
    REQUIRE(preview.entries.size() == entries.size());
    HeadlessAnalysisCommand::Options options;
    options.operation = HeadlessAnalysisCommand::Operation::CategorizeAndRename;
    options.paths = {source};
    const auto review_file = root / "plan.json";
    std::string error;
    REQUIRE(HeadlessStatusJson::write_review_plan_file(review_file, options, entries, apply_options, &error, &preview));
    QFile file(QString::fromStdString(Utils::path_to_utf8(review_file)));
    REQUIRE(file.open(QIODevice::ReadOnly));
    const auto document = QJsonDocument::fromJson(file.readAll());
    file.close();
    const auto saved = document.object().value(QStringLiteral("entries")).toArray();
    const auto status = HeadlessStatusJson::apply_result_to_json(preview).value(QStringLiteral("review"))
                            .toObject().value(QStringLiteral("entries")).toArray();
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const auto row = saved.at(static_cast<qsizetype>(index)).toObject();
        const auto outcome = status.at(static_cast<qsizetype>(index)).toObject();
        CHECK(row.value(QStringLiteral("plannedDestination")).toString().toStdString() == preview.entries[index].destination);
        CHECK(row.value(QStringLiteral("folderTreeRoutingExplanation")).toString().toStdString() == entries[index].folder_tree_routing_explanation);
        CHECK(outcome.value(QStringLiteral("folderTreeRoutingExplanation")).toString().toStdString() == preview.entries[index].folder_tree_routing_explanation);
        CHECK(outcome.value(QStringLiteral("folderTreeMode")).toBool() == entries[index].folder_tree_mode);
        if (!preview.entries[index].destination.empty()) {
            CHECK(row.value(QStringLiteral("suggestedName")).toString().toStdString() == preview.entries[index].destination_name);
        }
    }
    // A display-only path cannot redirect a saved plan when the app reads it back.
    auto edited = document.object();
    auto edited_entries = saved;
    auto edited_row = edited_entries[0].toObject();
    edited_row.insert(QStringLiteral("plannedDestination"), QStringLiteral("C:/untrusted/elsewhere.txt"));
    edited_entries[0] = edited_row;
    edited.insert(QStringLiteral("entries"), edited_entries);
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    REQUIRE(file.write(QJsonDocument(edited).toJson()) > 0);
    file.close();
    HeadlessStatusJson::ReviewPlan plan;
    REQUIRE(HeadlessStatusJson::read_review_plan_file(review_file, &plan, &error));
    const auto replay = service.apply(plan.entries, apply_options);
    REQUIRE(replay.entries.size() == preview.entries.size());
    for (std::size_t index = 0; index < entries.size(); ++index) {
        CHECK(replay.entries[index].destination == preview.entries[index].destination);
        CHECK(std::filesystem::exists(source / entries[index].file_name));
    }
    CHECK_FALSE(std::filesystem::exists(archive / "Pending"));
    CHECK_FALSE(std::filesystem::exists(archive / "Documents"));
    auto mismatched = preview;
    mismatched.entries.pop_back();
    CHECK_FALSE(HeadlessStatusJson::write_review_plan_file(root / "bad-count.json", options, entries, apply_options, &error, &mismatched));
    mismatched = preview;
    mismatched.entries[0].source = "unrelated.txt";
    CHECK_FALSE(HeadlessStatusJson::write_review_plan_file(root / "bad-source.json", options, entries, apply_options, &error, &mismatched));
    CHECK_FALSE(std::filesystem::exists(root / "bad-count.json"));
    CHECK_FALSE(std::filesystem::exists(root / "bad-source.json"));
}
