#include "FolderTreeLLMRouter.hpp"
#include "FolderStructurePattern.hpp"
#include "LLMErrors.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace {
std::string selected(const std::string& path, bool create = false)
{
    return "{\"targetFolder\":\"" + path + "\",\"createFolder\":" + (create ? "true}" : "false}");
}
}

TEST_CASE("FolderTreeLLMRouter compares all folders without lexical exclusions", "[llm-routing]")
{
    const int folder_count = GENERATE(64, 200);
    std::vector<FolderTreeCatalog::Entry> entries;
    for (int i = 0; i < folder_count; ++i) {
        std::ostringstream name;
        name << "Department " << std::setw(3) << std::setfill('0') << i;
        entries.push_back({name.str(), 1});
    }
    const FolderTreeCatalog::Catalog catalog(entries);
    std::size_t calls = 0;
    std::vector<bool> seen(entries.size(), false);
    const auto result = FolderTreeLLMRouter::select(catalog, true, "Misleading", "Labels", {}, [&](const auto& context) {
        ++calls;
        std::string first;
        for (std::size_t i = 0; i < entries.size(); ++i) {
            if (context.find(entries[i].relative_path + "\n") == std::string::npos) continue;
            seen[i] = true;
            if (first.empty()) first = entries[i].relative_path;
        }
        // The correct choice need not share any words with the file or preliminary labels.
        const auto& target = entries.back().relative_path;
        return selected(context.find(target + "\n") != std::string::npos ? target : first);
    });
    REQUIRE(result);
    CHECK(result->relative_path == entries.back().relative_path);
    if (folder_count == 64) CHECK(calls == 1);
    else CHECK(calls > 1);
    CHECK(std::all_of(seen.begin(), seen.end(), [](bool value) { return value; }));
}

TEST_CASE("FolderTreeLLMRouter splits intact lists on token-budget errors", "[llm-routing]")
{
    const FolderTreeCatalog::Catalog catalog({{"Alpha", 1}, {"Beta", 1}, {"Gamma", 1}, {"Delta", 1}});
    int budget_errors = 0;
    const auto result = FolderTreeLLMRouter::select(catalog, false, "", "", {}, [&](const auto& context) {
        if (context.find("Alpha\n") != std::string::npos && context.find("Beta\n") != std::string::npos &&
            context.find("Gamma\n") != std::string::npos) {
            ++budget_errors;
            throw PromptBudgetError("too many candidates");
        }
        return selected(context.find("Gamma\n") != std::string::npos ? "Gamma" : "Alpha");
    });
    REQUIRE(result);
    CHECK(result->relative_path == "Gamma");
    CHECK(budget_errors == 1);
    CHECK_THROWS_AS(FolderTreeLLMRouter::select(catalog, false, "", "", {}, [](const auto&) -> std::string {
        throw PromptBudgetError("mandatory content cannot fit");
    }), PromptBudgetError);
}

TEST_CASE("FolderTreeLLMRouter retries invalid responses without inventing a destination", "[llm-routing]")
{
    const FolderTreeCatalog::Catalog catalog({{"Finance/Taxes", 2}});
    int calls = 0;
    CHECK_FALSE(FolderTreeLLMRouter::select(catalog, false, "Finance", "Taxes", {}, [&](const auto&) {
        ++calls;
        return selected("../outside");
    }));
    CHECK(calls == 2);
    CHECK_FALSE(FolderTreeLLMRouter::select(catalog, false, "Finance", "Taxes", {}, [](const auto&) {
        return selected("Finance/Budget", true);
    }));
}

TEST_CASE("FolderTreeLLMRouter accepts plain existing and permitted new paths", "[llm-routing][plain-path]")
{
    const FolderTreeCatalog::Catalog catalog({{"Finance", 1}, {"Finance/Invoices", 2}});
    for (const bool allow_new : {false, true}) {
        const std::string target = allow_new ? "Finance/Forecasts" : "Finance/Invoices";
        const auto result = FolderTreeLLMRouter::select(catalog, allow_new, "Finance", "", {},
            [&](const auto& context) {
                CHECK(context.find("Return JSON with targetFolder") != std::string::npos);
                CHECK(context.find("createFolder (boolean)") != std::string::npos);
                return target;
            });
        REQUIRE(result);
        CHECK(result->relative_path == target);
        CHECK(result->suggested_new == allow_new);
        CHECK(result->exists == !allow_new);
    }
}

TEST_CASE("FolderTreeLLMRouter retries keep the JSON response contract", "[llm-routing][folder-response]")
{
    const FolderTreeCatalog::Catalog catalog({{"Finance/Invoices", 2}});
    int calls = 0;
    const auto result = FolderTreeLLMRouter::select(catalog, false, "", "", {}, [&](const auto& context) {
        ++calls;
        if (calls == 1) return std::string("../outside");
        CHECK(context.find("Return one JSON object") != std::string::npos);
        CHECK(context.find("Never suggest or invent a new folder; return createFolder:false") != std::string::npos);
        return std::string("Finance/Invoices");
    });
    REQUIRE(result);
    CHECK(result->relative_path == "Finance/Invoices");
    CHECK(calls == 2);
}

TEST_CASE("Folder proposal allocation preserves model labels and parents while reserving numbers", "[llm-routing]")
{
    const FolderTreeCatalog::Catalog catalog({{"10-19 Finance", 1}, {"10-19 Finance/11 Invoices", 2},
        {"10-19 Finance/12 Banking", 2}, {"20-29 Work", 1}, {"20-29 Work/21 Projects", 2},
        {"20-29 Work/21 Projects/21.01 Launch", 3}});
    CHECK(FolderStructurePattern::allocate_proposed_path(catalog, "10-19 Finance/12 Forecasts") == "10-19 Finance/13 Forecasts");
    CHECK(FolderStructurePattern::allocate_proposed_path(catalog, "20-29 Work/21 Projects/21.01 Research") ==
          "20-29 Work/21 Projects/21.02 Research");
    CHECK(FolderStructurePattern::allocate_proposed_path(catalog, "30-39 Media/31 Landscapes") == "30-39 Media/31 Landscapes");
    CHECK_FALSE(FolderStructurePattern::allocate_proposed_path(catalog, "../outside"));
    const FolderTreeCatalog::Catalog literal({{"Reports", 1}});
    CHECK(FolderStructurePattern::allocate_proposed_path(literal, "Reports/2024 Taxes") == "Reports/2024 Taxes");
}
