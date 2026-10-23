#include "FolderTreeLLMRouter.hpp"
#include "FolderStructurePattern.hpp"
#include "FolderTreeSelectionContract.hpp"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
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
    const auto result = FolderTreeLLMRouter::select(catalog, true, "Misleading", "Labels", {}, [&](const auto& context, const auto& constraint) {
        ++calls;
        const auto choices = QJsonDocument::fromJson(QByteArray::fromStdString(constraint.json_schema))
            .object()["properties"].toObject()["targetFolder"].toObject()["enum"].toArray();
        std::string first;
        for (std::size_t i = 0; i < entries.size(); ++i) {
            if (context.find(entries[i].relative_path + "\n") == std::string::npos) continue;
            seen[i] = true;
            if (first.empty()) first = entries[i].relative_path;
            CHECK(choices.contains(QString::fromStdString(entries[i].relative_path)));
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
    const auto result = FolderTreeLLMRouter::select(catalog, false, "", "", {}, [&](const auto& context, const auto&) {
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
    CHECK_THROWS_AS(FolderTreeLLMRouter::select(catalog, false, "", "", {}, [](const auto&, const auto&) -> std::string {
        throw PromptBudgetError("mandatory content cannot fit");
    }), PromptBudgetError);
}

TEST_CASE("FolderTreeLLMRouter retries invalid responses without inventing a destination", "[llm-routing]")
{
    const FolderTreeCatalog::Catalog catalog({{"Finance/Taxes", 2}});
    int calls = 0;
    CHECK_FALSE(FolderTreeLLMRouter::select(catalog, true, "Finance", "Taxes", {}, [&](const auto&, const auto&) {
        ++calls;
        return selected("../outside");
    }));
    CHECK(calls == 2);
    CHECK_FALSE(FolderTreeLLMRouter::select(catalog, false, "Finance", "Taxes", {}, [](const auto&, const auto&) {
        return selected("Finance/Budget", true);
    }));
}

TEST_CASE("FolderTreeLLMRouter proposes only after explicit existing-folder abstention", "[llm-routing][folder-response]")
{
    const FolderTreeCatalog::Catalog catalog({{"Finance", 1}, {"Finance/Invoices", 2}});
    for (const bool allow_new : {false, true}) {
        const std::string target = allow_new ? "Finance/Forecasts" : "Finance/Invoices";
        int calls = 0;
        const auto result = FolderTreeLLMRouter::select(catalog, allow_new, "Finance", "", {},
            [&](const auto& context, const auto&) {
                ++calls;
                CHECK(context.find("Return JSON with targetFolder") != std::string::npos);
                CHECK(context.find("createFolder (boolean)") != std::string::npos);
                if (allow_new && calls == 1) return std::string(R"({"targetFolder":null,"createFolder":false})");
                CHECK((context.find(FolderTreeSelectionContract::kProposalMarker) != std::string::npos) == allow_new);
                return selected(target, allow_new);
            });
        REQUIRE(result);
        CHECK(result->relative_path == target);
        CHECK(result->suggested_new == allow_new);
        CHECK(result->exists == !allow_new);
        CHECK(calls == (allow_new ? 2 : 1));
    }
}

TEST_CASE("FolderTreeLLMRouter retries keep the JSON response contract", "[llm-routing][folder-response]")
{
    const FolderTreeCatalog::Catalog catalog({{"Finance/Invoices", 2}});
    int calls = 0;
    const auto result = FolderTreeLLMRouter::select(catalog, false, "", "", {}, [&](const auto& context, const auto&) {
        ++calls;
        if (calls == 1) return std::string("../outside");
        CHECK(context.find("return exactly targetFolder") != std::string::npos);
        CHECK(context.find("Never suggest or invent a new folder; return createFolder:false") != std::string::npos);
        return selected("Finance/Invoices");
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

TEST_CASE("FolderTreeLLMRouter corrects nonexistent existing targets when creation is allowed", "[llm-routing]")
{
    const FolderTreeCatalog::Catalog catalog({{"70-79 Media & Design", 1},
        {"70-79 Media & Design/74 Presentations", 2}});
    int calls = 0;
    const auto result = FolderTreeLLMRouter::select(catalog, true, "", "", {}, [&](const auto& context, const auto&) {
        if (++calls == 1) return selected("40-79 Media & Design/74 Presentations");
        CHECK(context.find("Copy one listed literal path exactly") != std::string::npos);
        return selected("70-79 Media & Design/74 Presentations");
    });
    REQUIRE(result);
    CHECK(calls == 2);
    CHECK(result->relative_path == "70-79 Media & Design/74 Presentations");
}

TEST_CASE("FolderTreeLLMRouter retries structural rejection before accepting a proposal", "[llm-routing]")
{
    std::vector<FolderTreeCatalog::Entry> entries;
    for (int start = 0; start <= 90; start += 10) {
        std::ostringstream area;
        area << std::setw(2) << std::setfill('0') << start << '-'
             << std::setw(2) << start + 9 << (start == 10 ? " Finance" : " Area");
        entries.push_back({area.str(), 1});
    }
    entries.push_back({"10-19 Finance/11 Invoices", 2});
    const FolderTreeCatalog::Catalog catalog(entries);
    int calls = 0;
    const auto result = FolderTreeLLMRouter::select(catalog, true, "", "", {}, [&](const auto&, const auto&) {
        if (++calls == 1) return std::string(R"({"targetFolder":null,"createFolder":false})");
        return calls == 2 ? selected("00-09 Finance/96 Forecasts", true)
                            : selected("10-19 Finance/12 Forecasts", true);
    }, [&](auto candidate) -> std::optional<FolderTreeCatalog::Selection> {
        const auto allocated = FolderStructurePattern::allocate_proposed_path(catalog, candidate.relative_path);
        return allocated ? FolderTreeCatalog::resolve_target_path(*allocated, catalog, true) : std::nullopt;
    });
    REQUIRE(result);
    CHECK(calls == 3);
    CHECK(result->relative_path == "10-19 Finance/12 Forecasts");
}

TEST_CASE("FolderTreeLLMRouter keeps abstention distinct from invalid replies", "[llm-routing]")
{
    const FolderTreeCatalog::Catalog catalog({{"Finance", 1}});
    const bool allow_new = GENERATE(false, true);
    const auto response = GENERATE(std::string("Finance"), std::string(R"({"targetFolder":null,"createFolder":false})"));
    int calls = 0;
    const auto result = FolderTreeLLMRouter::select(catalog, allow_new, "", "", {},
        [&](const auto& context, const auto&) {
            ++calls;
            if (context.find(FolderTreeSelectionContract::kProposalMarker) != std::string::npos) {
                return selected("Research", true);
            }
            return response;
        });
    CHECK(static_cast<bool>(result) == (allow_new && response != "Finance"));
    CHECK(calls == (response == "Finance" || allow_new ? 2 : 1));
}

TEST_CASE("FolderTreeLLMRouter handles empty catalogs and group abstentions", "[llm-routing]")
{
    std::vector<FolderTreeCatalog::Entry> entries;
    const int count = GENERATE(0, 200);
    for (int i = 0; i < count; ++i) entries.push_back({"Folder " + std::to_string(i), 1});
    const FolderTreeCatalog::Catalog catalog(entries);
    int proposals = 0;
    int comparisons = 0;
    const bool one_match = count > 0 && GENERATE(false, true);
    const auto result = FolderTreeLLMRouter::select(catalog, true, "", "", {},
        [&](const auto& context, const auto&) {
            if (context.find(FolderTreeSelectionContract::kProposalMarker) != std::string::npos) {
                ++proposals;
                return selected("Research", true);
            }
            ++comparisons;
            return one_match && context.find("Folder 199\n") != std::string::npos
                ? selected("Folder 199") : std::string(R"({"targetFolder":null,"createFolder":false})");
        });
    REQUIRE(result);
    CHECK(result->relative_path == (one_match ? "Folder 199" : "Research"));
    CHECK(proposals == (one_match ? 0 : 1));
    CHECK(comparisons == (count == 0 ? 0 : 2));
}

TEST_CASE("FolderTreeLLMRouter never proposes after one comparison group fails validation", "[llm-routing]")
{
    std::vector<FolderTreeCatalog::Entry> entries;
    for (int i = 0; i < 200; ++i) entries.push_back({"Folder " + std::to_string(i), 1});
    const FolderTreeCatalog::Catalog catalog(entries);
    int proposals = 0;
    const auto selection = FolderTreeLLMRouter::select(catalog, true, "", "", {},
        [&](const auto& context, const auto&) {
            if (context.find(FolderTreeSelectionContract::kProposalMarker) != std::string::npos) ++proposals;
            return context.find("Folder 0\n") != std::string::npos
                ? std::string("malformed reply") : std::string(R"({"targetFolder":null,"createFolder":false})");
        });
    CHECK_FALSE(selection);
    CHECK(proposals == 0);
}

TEST_CASE("FolderTreeLLMRouter reconsiders an existing path named during proposal", "[llm-routing]")
{
    const FolderTreeCatalog::Catalog catalog({{"70-79 Media & Design/74 Presentations", 2}});
    int calls = 0;
    const auto selection = FolderTreeLLMRouter::select(catalog, true, "Documents", "Presentations", {},
        [&](const auto& context, const auto&) {
            if (++calls == 1) return std::string(R"({"targetFolder":null,"createFolder":false})");
            CHECK((context.find(FolderTreeSelectionContract::kReconsiderationMarker) != std::string::npos) == (calls == 3));
            return selected("70-79 Media & Design/74 Presentations", calls == 2);
        }, {}, {false, true});
    REQUIRE(selection);
    CHECK(selection->relative_path == "70-79 Media & Design/74 Presentations");
    CHECK(selection->exists);
    CHECK_FALSE(selection->suggested_new);
    CHECK(calls == 3);
}

TEST_CASE("FolderTreeLLMRouter does not approve a contradictory proposal without a supported selection", "[llm-routing]")
{
    const std::string response = GENERATE(std::string("invalid"), std::string(R"({"targetFolder":null,"createFolder":false})"));
    const FolderTreeCatalog::Catalog catalog({{"Reports", 1, "Board reports"}, {"Presentations", 1, "Talks and slide decks"}});
    int calls = 0;
    const auto result = FolderTreeLLMRouter::select(catalog, true, "Marketing", "Launch", {},
        [&](const auto& context, const auto&) {
            if (++calls == 1) return std::string(R"({"targetFolder":null,"createFolder":false})");
            if (calls == 2) return selected("Reports", true);
            CHECK(context.find(FolderTreeSelectionContract::kReconsiderationMarker) != std::string::npos);
            CHECK(context.find("Board reports") != std::string::npos);
            CHECK(context.find("Talks and slide decks") != std::string::npos);
            return response;
        }, {}, {false, true});
    CHECK_FALSE(result);
    CHECK(calls == (response == "invalid" ? 4 : 3));
}

TEST_CASE("FolderTreeLLMRouter can revise a contradictory choice using folder purposes", "[llm-routing]")
{
    const FolderTreeCatalog::Catalog catalog({{"Reports", 1, "Board reports"}, {"Presentations", 1, "Talks and slide decks"}});
    int calls = 0;
    const auto result = FolderTreeLLMRouter::select(catalog, true, "Business", "Launch", {},
        [&](const auto& context, const auto&) {
            ++calls;
            CHECK(context.find("Purpose-aware comparison") != std::string::npos);
            if (calls == 1) return std::string(R"({"targetFolder":null,"createFolder":false})");
            return calls == 2 ? selected("Reports", true) : selected("Presentations");
        }, {}, {true, true});
    REQUIRE(result);
    CHECK(result->relative_path == "Presentations");
    CHECK(calls == 3);
}

TEST_CASE("FolderTreeLLMRouter preserves purpose descriptions in tournament finalists", "[llm-routing][folder-purpose]")
{
    std::vector<FolderTreeCatalog::Entry> entries;
    for (int i = 0; i < 200; ++i) entries.push_back({"Folder " + std::to_string(i), 1, "Purpose " + std::to_string(i)});
    const FolderTreeCatalog::Catalog catalog(entries);
    int calls = 0;
    const auto result = FolderTreeLLMRouter::select(catalog, false, "", "", {}, [&](const auto& context, const auto& constraint) {
        ++calls;
        const auto schema = QJsonDocument::fromJson(QByteArray::fromStdString(constraint.json_schema)).object();
        const auto choices = schema["properties"].toObject()["targetFolder"].toObject()["enum"].toArray();
        for (const auto& choice : choices) {
            if (choice.isString()) CHECK(context.find("Purpose " + choice.toString().mid(QString("Folder ").size()).toStdString()) != std::string::npos);
        }
        return selected(choices.first().toString().toStdString());
    });
    REQUIRE(result);
    CHECK(calls == 3);
}

TEST_CASE("FolderTreeLLMRouter defaults retain descriptions and reuse a validated existing proposal", "[llm-routing]")
{
    const FolderTreeCatalog::Catalog catalog({{"Reports", 1, "Board reports"}});
    int calls = 0;
    const auto result = FolderTreeLLMRouter::select(catalog, true, "", "", {},
        [&](const auto& context, const auto&) {
            CHECK(context.find("Purpose-aware comparison") == std::string::npos);
            CHECK(context.find("Board reports") != std::string::npos);
            return ++calls == 1 ? std::string(R"({"targetFolder":null,"createFolder":false})") : selected("Reports", true);
        }, [](auto selection) -> std::optional<FolderTreeCatalog::Selection> {
            CHECK(selection.relative_path == "Reports");
            CHECK(selection.exists);
            CHECK_FALSE(selection.suggested_new);
            return selection;
        });
    REQUIRE(result);
    CHECK(result->exists);
    CHECK(calls == 2);
}

TEST_CASE("FolderTreeLLMRouter comparison controls allow isolated ablations", "[llm-routing]")
{
    const bool purpose = GENERATE(false, true);
    const bool reconsider = GENERATE(false, true);
    const FolderTreeCatalog::Catalog catalog({{"Reports", 1, "Board reports"}});
    int calls = 0;
    const auto result = FolderTreeLLMRouter::select(catalog, true, "", "", {},
        [&](const auto& context, const auto&) {
            ++calls;
            CHECK((context.find("Purpose-aware comparison") != std::string::npos) == purpose);
            CHECK(context.find("Board reports") != std::string::npos);
            CHECK((context.find(FolderTreeSelectionContract::kReconsiderationMarker) != std::string::npos) == (calls == 3));
            return calls == 2 ? selected("Reports", true)
                             : std::string(R"({"targetFolder":null,"createFolder":false})");
        }, {}, {purpose, reconsider});
    CHECK(static_cast<bool>(result) == !reconsider);
    CHECK(calls == (reconsider ? 3 : 2));
}
