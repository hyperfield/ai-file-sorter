#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Shared fixtures for large-whitelist developer and test-mode checks.
 */
class WhitelistTestFixtures {
public:
    /**
     * @brief File created for a whitelist test preset.
     */
    struct SampleFile {
        /** @brief File name to create in the sample directory. */
        std::string file_name;
        /** @brief Lightweight text payload written to the file. */
        std::string contents;
        /** @brief Expected broad category, shown to developers for manual review. */
        std::string expected_category;
    };

    /**
     * @brief Complete large-whitelist preset.
     */
    struct Preset {
        /** @brief Human-readable whitelist name used in the UI selector. */
        std::string whitelist_name;
        /** @brief Large category set used to exercise prompt candidate reduction. */
        std::vector<std::string> categories;
        /** @brief Optional subcategory constraints; empty means subcategories are unrestricted. */
        std::vector<std::string> subcategories;
        /** @brief Optional category-specific subcategory constraints. */
        std::unordered_map<std::string, std::vector<std::string>> subcategories_by_category;
        /** @brief Sample files to materialize for the test run. */
        std::vector<SampleFile> files;
    };

    /**
     * @brief Return the standard large-whitelist LLM test preset.
     * @return Preset with a large category list and representative sample files.
     */
    static Preset large_whitelist_preset();

    /**
     * @brief Create or refresh sample files for a preset.
     * @param directory Destination directory.
     * @param preset Preset whose sample files should be written.
     */
    static void write_sample_files(const std::filesystem::path& directory,
                                   const Preset& preset);
};
