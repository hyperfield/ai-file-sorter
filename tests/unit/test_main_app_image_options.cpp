#include <catch2/catch_test_macros.hpp>

#include "MainApp.hpp"
#include "MainAppTestAccess.hpp"
#include "Settings.hpp"
#include "TestHelpers.hpp"
#include "Utils.hpp"

#include <QCheckBox>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <vector>

#ifndef _WIN32
namespace {

void create_visual_llm_files()
{
    const std::string model_url = "https://example.com/llava-model.gguf";
    const std::string mmproj_url = "https://example.com/mmproj-model-f16.gguf";

    const std::filesystem::path model_path =
        Utils::make_default_path_to_file_from_download_url(model_url);
    const std::filesystem::path mmproj_path =
        Utils::make_default_path_to_file_from_download_url(mmproj_url);

    std::filesystem::create_directories(model_path.parent_path());
    std::ofstream(model_path.string(), std::ios::binary).put('x');
    std::ofstream(mmproj_path.string(), std::ios::binary).put('x');
}

} // namespace

TEST_CASE("Image analysis checkboxes enable and enforce rename-only behavior") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    EnvVarGuard model_guard("LLAVA_MODEL_URL", std::string("https://example.com/llava-model.gguf"));
    EnvVarGuard mmproj_guard("LLAVA_MMPROJ_URL", std::string("https://example.com/mmproj-model-f16.gguf"));

    create_visual_llm_files();

    Settings settings;
    settings.set_analyze_images_by_content(false);
    settings.set_offer_rename_images(false);
    settings.set_rename_images_only(false);
    REQUIRE(settings.save());

    MainApp window(settings, /*development_mode=*/false);

    QCheckBox* analyze = MainAppTestAccess::analyze_images_checkbox(window);
    QCheckBox* process_only = MainAppTestAccess::process_images_only_checkbox(window);
    QCheckBox* offer = MainAppTestAccess::offer_rename_images_checkbox(window);
    QCheckBox* rename_only = MainAppTestAccess::rename_images_only_checkbox(window);

    REQUIRE(analyze != nullptr);
    REQUIRE(process_only != nullptr);
    REQUIRE(offer != nullptr);
    REQUIRE(rename_only != nullptr);

    REQUIRE_FALSE(analyze->isChecked());
    REQUIRE_FALSE(process_only->isEnabled());
    REQUIRE_FALSE(offer->isEnabled());
    REQUIRE_FALSE(rename_only->isEnabled());

    analyze->setChecked(true);
    REQUIRE(process_only->isEnabled());
    REQUIRE(offer->isEnabled());
    REQUIRE(rename_only->isEnabled());

    rename_only->setChecked(true);
    REQUIRE(offer->isChecked());

    offer->setChecked(false);
    REQUIRE_FALSE(rename_only->isChecked());
}

TEST_CASE("Image analysis toggle disables when dialog closes without downloads") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    EnvVarGuard model_guard("LLAVA_MODEL_URL", std::string("https://example.com/llava-model.gguf"));
    EnvVarGuard mmproj_guard("LLAVA_MMPROJ_URL", std::string("https://example.com/mmproj-model-f16.gguf"));

    Settings settings;
    settings.set_analyze_images_by_content(false);
    settings.set_offer_rename_images(false);
    settings.set_rename_images_only(false);
    REQUIRE(settings.save());

    MainApp window(settings, /*development_mode=*/false);
    MainAppTestAccess::set_visual_llm_available_probe(window, []() { return false; });
    MainAppTestAccess::set_llm_selection_runner(window, []() {});
    MainAppTestAccess::set_image_analysis_prompt_override(window, []() { return true; });

    QCheckBox* analyze = MainAppTestAccess::analyze_images_checkbox(window);
    REQUIRE(analyze != nullptr);

    analyze->setChecked(true);
    REQUIRE_FALSE(analyze->isChecked());
    REQUIRE_FALSE(settings.get_analyze_images_by_content());
}

TEST_CASE("Image analysis toggle cancels when user declines download") {
    EnvVarGuard platform_guard("QT_QPA_PLATFORM", std::string("offscreen"));
    QtAppContext qt_context;

    TempDir temp;
    EnvVarGuard home_guard("HOME", temp.path().string());
    EnvVarGuard config_guard("AI_FILE_SORTER_CONFIG_DIR", temp.path().string());
    EnvVarGuard model_guard("LLAVA_MODEL_URL", std::string("https://example.com/llava-model.gguf"));
    EnvVarGuard mmproj_guard("LLAVA_MMPROJ_URL", std::string("https://example.com/mmproj-model-f16.gguf"));

    Settings settings;
    settings.set_analyze_images_by_content(false);
    settings.set_offer_rename_images(false);
    settings.set_rename_images_only(false);
    REQUIRE(settings.save());

    bool dialog_opened = false;
    MainApp window(settings, /*development_mode=*/false);
    MainAppTestAccess::set_visual_llm_available_probe(window, []() { return false; });
    MainAppTestAccess::set_llm_selection_runner(window, [&dialog_opened]() { dialog_opened = true; });
    MainAppTestAccess::set_image_analysis_prompt_override(window, []() { return false; });

    QCheckBox* analyze = MainAppTestAccess::analyze_images_checkbox(window);
    REQUIRE(analyze != nullptr);

    analyze->setChecked(true);
    REQUIRE_FALSE(analyze->isChecked());
    REQUIRE_FALSE(settings.get_analyze_images_by_content());
    REQUIRE_FALSE(dialog_opened);
}

TEST_CASE("Already-renamed images skip vision analysis") {
    std::vector<FileEntry> files = {
        {"/tmp/renamed.png", "renamed.png", FileType::File},
        {"/tmp/other.png", "other.png", FileType::File},
        {"/tmp/doc.txt", "doc.txt", FileType::File},
        {"/tmp/folder", "folder", FileType::Directory}
    };
    std::unordered_set<std::string> renamed_files = {"renamed.png"};

    auto contains = [](const std::vector<FileEntry>& entries, const std::string& name) {
        return std::any_of(entries.begin(),
                           entries.end(),
                           [&name](const FileEntry& entry) { return entry.file_name == name; });
    };

    std::vector<FileEntry> image_entries;
    std::vector<FileEntry> other_entries;

    SECTION("categorization uses filename when already renamed") {
        MainAppTestAccess::split_entries_for_analysis(files,
                                                      true,
                                                      false,
                                                      false,
                                                      renamed_files,
                                                      image_entries,
                                                      other_entries);

        CHECK_FALSE(contains(image_entries, "renamed.png"));
        CHECK(contains(other_entries, "renamed.png"));
        CHECK(contains(image_entries, "other.png"));
        CHECK(contains(other_entries, "doc.txt"));
        CHECK(contains(other_entries, "folder"));
    }

    SECTION("rename-only skips already-renamed images entirely") {
        MainAppTestAccess::split_entries_for_analysis(files,
                                                      true,
                                                      false,
                                                      true,
                                                      renamed_files,
                                                      image_entries,
                                                      other_entries);

        CHECK_FALSE(contains(image_entries, "renamed.png"));
        CHECK_FALSE(contains(other_entries, "renamed.png"));
        CHECK(contains(image_entries, "other.png"));
    }
}
#endif
