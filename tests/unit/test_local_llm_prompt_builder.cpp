#include <catch2/catch_test_macros.hpp>

#include "FolderTreeCatalog.hpp"
#include "LocalLLMPromptBuilder.hpp"
#include "ILLMClient.hpp"

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

    const std::string folder_tree_context =
        std::string(FolderTreeCatalog::kPromptMarker) +
        "\n- If the listed folders are only weak, generic, or unrelated matches, "
        "suggest a concise new folder.";
    const std::string folder_tree_system_prompt =
        LocalLLMPromptBuilder::build_system_prompt("home/theuser/Downloads/report.pdf",
                                                   FileType::File,
                                                   folder_tree_context);
    CHECK(folder_tree_system_prompt == FolderTreeCatalog::selection_system_prompt());
    CHECK(folder_tree_system_prompt.find("Reply with exactly one JSON object") != std::string::npos);
    CHECK(folder_tree_system_prompt.find("\"targetFolder\":\"relative/folder/path\",\"createFolder\":false") != std::string::npos);
    CHECK(folder_tree_system_prompt.find("separate new-folder proposal stage") !=
          std::string::npos);
    CHECK(folder_tree_system_prompt.find("no listed folder is suitable") != std::string::npos);

    const std::string folder_tree_user_prompt =
        LocalLLMPromptBuilder::build_user_prompt("report.pdf",
                                                 "home/theuser/Downloads/report.pdf",
                                                 FileType::File,
                                                 folder_tree_context);
    CHECK(folder_tree_user_prompt.find(FolderTreeCatalog::selection_response_instruction()) != std::string::npos);
    CHECK(folder_tree_user_prompt.find("\"createFolder\":true") != std::string::npos);
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

TEST_CASE("LocalLLMPromptBuilder uses JSON answers for every folder-routing input", "[folder-response]")
{
    const std::string context(FolderTreeCatalog::kPromptMarker);
    for (const auto* path : {"/inbox/report.pdf\nDocument summary: Budget report.",
                            "/inbox/photo.png\nImage description: A receipt.", "/inbox/installer.deb"}) {
        const auto system = LocalLLMPromptBuilder::build_system_prompt(path, FileType::File, context);
        const auto user = LocalLLMPromptBuilder::build_user_prompt("item", path, FileType::File, context);
        CHECK(system == FolderTreeCatalog::selection_system_prompt());
        CHECK(user.ends_with(FolderTreeCatalog::selection_response_instruction()));
        CHECK(user.find("\"createFolder\":false") != std::string::npos);
        CHECK(user.find("\"createFolder\":true") != std::string::npos);
    }
    const auto directory = LocalLLMPromptBuilder::build_user_prompt("Reports", "/inbox/Reports", FileType::Directory, context);
    CHECK(directory.ends_with(FolderTreeCatalog::selection_response_instruction()));
}

TEST_CASE("LocalLLMPromptBuilder shortening preserves the folder-response footer", "[folder-response][prompt-budget]")
{
    const std::string footer(FolderTreeCatalog::selection_response_instruction());
    const auto prompt = "Document summary: " + std::string(5000, 'x') + "\n" + footer;
    // Use a normal prompt prefix so the summary marker starts after a newline.
    const auto input = "Categorize this document.\n" + prompt;
    const auto shrunk = LocalLLMPromptBuilder::shrink_user_prompt_for_context(input, 3);
    CHECK(shrunk.size() < input.size());
    CHECK(shrunk.ends_with(footer));
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

TEST_CASE("LocalLLMPromptBuilder retains content boundaries and folder constraints on retries", "[content-routing]")
{
    for (const auto* label : {"Document summary: ", "Image description: "}) {
        const std::string context = std::string("Content evidence (primary):\n") + label +
            std::string(5000, 'a') + std::string(LLMPrompt::kContentEvidenceEndMarker) + "\n" +
            std::string(FolderTreeCatalog::kPromptMarker) +
            "\nExisting destination folder candidates:\n80-89 Legal/82 Data Protection\n";
        const auto prompt = LocalLLMPromptBuilder::build_user_prompt(
            "policy.pdf", "/home/theuser/policy.pdf", FileType::File, context);
        for (int attempt = 0; attempt < 4; ++attempt) {
            const auto shrunk = LocalLLMPromptBuilder::shrink_user_prompt_for_context(prompt, attempt);
            CHECK(shrunk.size() < prompt.size());
            CHECK(shrunk.find(LLMPrompt::kContentEvidenceEndMarker) != std::string::npos);
            CHECK(shrunk.find("80-89 Legal/82 Data Protection") != std::string::npos);
            CHECK(shrunk.find("Answer with exactly one JSON object") != std::string::npos);
        }
    }
}

TEST_CASE("LocalLLMPromptBuilder recognizes evidence supplied separately from the path", "[content-routing]")
{
    for (const bool image : {false, true}) {
        const std::string name = image ? "dashboard.png" : "report.pdf";
        const std::string path = "/home/theuser/Downloads/" + name;
        const std::string context = std::string("Content evidence (primary):\n") +
            (image ? "Image description: Dashboard with charts." : "Document summary: Privacy policy.") +
            std::string(LLMPrompt::kContentEvidenceEndMarker);
        const auto system = LocalLLMPromptBuilder::build_system_prompt(path, FileType::File, context);
        const auto user = LocalLLMPromptBuilder::build_user_prompt(name, path, FileType::File, context);
        CHECK(system.find("primary evidence") != std::string::npos);
        CHECK(system.find("name only") == std::string::npos);
        CHECK(system.find(image ? "You categorize image files" : "document categorization assistant") !=
              std::string::npos);
        CHECK(user.find(image ? "Categorize this image file" : "Categorize this document file") == 0);
        CHECK(user.find(context) != std::string::npos);
    }
}

TEST_CASE("LocalLLMPromptBuilder fits complete chat prompts beyond an unchanged shrink step", "[prompt-budget]")
{
    const std::string prefix = "<BOS><SYSTEM>mandatory instructions</SYSTEM><USER>";
    const std::string suffix = "</USER><ASSISTANT>";
    const auto prompt = std::string("Document\nDocument summary: ") + std::string(900, 'x') +
        std::string(LLMPrompt::kContentEvidenceEndMarker) + "\nAnswer with exactly one JSON object.";
    CHECK(LocalLLMPromptBuilder::shrink_user_prompt_for_context(prompt, 0) == prompt);
    const auto fitted = LocalLLMPromptBuilder::fit_user_prompt(prompt, 700,
        [&](const auto& user) { return (prefix + user + suffix).size(); });
    REQUIRE(fitted);
    CHECK((prefix + *fitted + suffix).size() <= 700);
    CHECK(fitted->find(LLMPrompt::kContentEvidenceEndMarker) != std::string::npos);
    CHECK(fitted->ends_with("Answer with exactly one JSON object."));
}

TEST_CASE("LocalLLMPromptBuilder rejects budgets that would truncate required instructions", "[prompt-budget]")
{
    const std::string prompt = std::string(2000, 'x') + "\nAllowed main categories: A, B\nAnswer with exactly one line.";
    CHECK_FALSE(LocalLLMPromptBuilder::fit_user_prompt(prompt, 700,
        [](const auto& user) { return user.size() + 100; }));
    CHECK(LocalLLMPromptBuilder::shrink_user_prompt_for_context(prompt, 3) == prompt);
}
