#include <catch2/catch_test_macros.hpp>

#include "CategoryLanguage.hpp"
#include "HeadlessStatusJson.hpp"
#include "Utils.hpp"

#include <QTemporaryDir>

#include <filesystem>
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
    CHECK(loaded.folder_tree_mode);
    CHECK(loaded.target_folder_suggested_new);
    CHECK_FALSE(loaded.target_folder_exists);
    CHECK(loaded.folder_tree_allow_new_folders);
    CHECK(loaded.rename_only);
}
