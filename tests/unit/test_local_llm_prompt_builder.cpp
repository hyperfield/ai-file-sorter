#include <catch2/catch_test_macros.hpp>

#include "LocalLLMPromptBuilder.hpp"

#include <string>

TEST_CASE("LocalLLMPromptBuilder preserves specialized prompt routing")
{
    const std::string image_path =
        "home/theuser/Downloads/dashboard.png\n"
        "Image description: Dashboard screenshot with charts and controls.";
    const std::string image_prompt =
        LocalLLMPromptBuilder::build_system_prompt(image_path, FileType::File);

    CHECK(image_prompt.find("always use Images as the main category") != std::string::npos);
    CHECK(image_prompt.find("Reply with exactly one line in the format Images : <Subcategory>") != std::string::npos);

    const std::string document_prompt =
        LocalLLMPromptBuilder::build_system_prompt("home/theuser/Downloads/report.pdf", FileType::File);
    CHECK(document_prompt.find("document categorization assistant") != std::string::npos);
    CHECK(document_prompt.find("If the file is an installer") == std::string::npos);

    const std::string directory_prompt =
        LocalLLMPromptBuilder::build_system_prompt("home/theuser/Downloads/Invoices", FileType::Directory);
    CHECK(directory_prompt.find("directory categorization assistant") != std::string::npos);
}

TEST_CASE("LocalLLMPromptBuilder strips image guidance from image prompts only")
{
    const std::string consistency_context =
        "Image categorization guidance:\n"
        "- Internal guidance should not be duplicated.\n\n"
        "Allowed main categories (pick exactly one label from the numbered list):\n"
        "1) Images\n"
        "2) Documents";
    const std::string prompt = LocalLLMPromptBuilder::build_user_prompt(
        "dashboard.png",
        "home/theuser/Downloads/dashboard.png\n"
        "Image description: Dashboard screenshot with charts.",
        FileType::File,
        consistency_context);

    CHECK(prompt.find("Categorize this image file for file organization.") == 0);
    CHECK(prompt.find("Image categorization guidance:") == std::string::npos);
    CHECK(prompt.find("Allowed main categories") != std::string::npos);
    CHECK(prompt.find("Full path:") == std::string::npos);
}

TEST_CASE("LocalLLMPromptBuilder shrinks long analysis sections before truncating prompt")
{
    const std::string long_summary(1600, 'a');
    const std::string prompt =
        "Categorize this document file for file organization.\n\n"
        "File name: quarterly_report.pdf\n"
        "Path: home/theuser/Downloads/quarterly_report.pdf\n"
        "Document summary: " + long_summary +
        "\nAllowed main categories (pick exactly one label from the numbered list):\n"
        "1) Documents\n"
        "\nAnswer with exactly one line:\n<Main category> : <Subcategory>";

    const std::string shrunk = LocalLLMPromptBuilder::shrink_user_prompt_for_context(prompt, 1);

    CHECK(shrunk.size() < prompt.size());
    CHECK(shrunk.find("Document summary: ") != std::string::npos);
    CHECK(shrunk.find("...") != std::string::npos);
    CHECK(shrunk.find("Allowed main categories") != std::string::npos);
    CHECK(shrunk.find("Answer with exactly one line") != std::string::npos);
}
