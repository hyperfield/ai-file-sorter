#include "FolderTreeSelectionContract.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using namespace FolderTreeSelectionContract;

TEST_CASE("Existing-folder schema contains only literal offered paths and abstention", "[folder-constraint]")
{
    const FolderTreeCatalog::Catalog catalog({{"70-79 Media & Design/74 Presentations", 2},
        {"Research/Quotes \"and\" caf\xc3\xa9", 2}});
    const auto schema = QJsonDocument::fromJson(QByteArray::fromStdString(
        existing_constraint(catalog.entries()).json_schema)).object();
    CHECK_FALSE(schema["additionalProperties"].toBool());
    CHECK(schema["required"].toArray().size() == 2);
    const auto properties = schema["properties"].toObject();
    const auto choices = properties["targetFolder"].toObject()["enum"].toArray();
    REQUIRE(choices.size() == 3);
    CHECK(choices.last().isNull());
    CHECK(choices[0].toString() == "70-79 Media & Design/74 Presentations");
    CHECK(choices[1].toString().toStdString() == catalog.entries()[1].relative_path);
    CHECK_FALSE(choices.contains("40-79 Media & Design/74 Presentations"));
    CHECK(properties["createFolder"].toObject()["const"] == false);
    const auto subset = existing_constraint(std::span(catalog.entries()).first(1));
    CHECK(subset.json_schema.find("Research") == std::string::npos);
    CHECK(existing_constraint({}).json_schema.find("[null]") != std::string::npos);
}

TEST_CASE("Existing-folder contract round trips escaped Unicode literal names", "[folder-constraint]")
{
    const std::string path = "Research/Caf\xc3\xa9 & Notes";
    const FolderTreeCatalog::Catalog catalog({{path, 2}});
    const std::string response = R"({"targetFolder":"Research/Caf\u00e9 & Notes","createFolder":false})";
    const auto decision = parse_existing(response, catalog);
    CHECK(decision.outcome == Outcome::Selected);
    REQUIRE(decision.selection);
    CHECK(decision.selection->relative_path == path);
}

TEST_CASE("Existing-folder replies require exact JSON choices without repairs", "[folder-constraint]")
{
    const FolderTreeCatalog::Catalog catalog({{"Finance/Invoices", 2}});
    const std::string response = GENERATE(
        "Finance/Invoices", "```json\n{\"targetFolder\":\"Finance/Invoices\",\"createFolder\":false}\n```",
        R"({"targetFolder":"finance/invoices","createFolder":false})",
        R"({"targetFolder":"Finance/Invoices/","createFolder":false})",
        R"({"targetFolder":"Finance/Forecasts","createFolder":false})",
        R"({"targetFolder":"Finance/Invoices","createFolder":true})",
        R"({"targetFolder":"Finance/Invoices","createFolder":"false"})",
        R"({"targetFolder":"Finance/Invoices"})", R"({"targetFolder":null})",
        R"({"targetFolder":false,"createFolder":false})", R"({"targetFolder":42,"createFolder":false})",
        R"({"targetFolder":"","createFolder":false})",
        R"({"targetFolder":"Finance/Invoices","createFolder":false,"reason":"extra"})");
    CHECK(parse_existing(response, catalog).outcome == Outcome::Invalid);
}

TEST_CASE("Existing-folder replies preserve null as a deliberate no-match outcome", "[folder-constraint]")
{
    const FolderTreeCatalog::Catalog catalog({{"Finance/Invoices", 2}});
    const auto abstain = parse_existing(R"({"targetFolder":null,"createFolder":false})", catalog);
    CHECK(abstain.outcome == Outcome::NoMatch);
    CHECK_FALSE(abstain.selection);
    const auto selected = parse_existing(R"({"targetFolder":"Finance/Invoices","createFolder":false})", catalog);
    CHECK(selected.outcome == Outcome::Selected);
    REQUIRE(selected.selection);
    CHECK(selected.selection->exists);
    CHECK_FALSE(selected.selection->suggested_new);
}

TEST_CASE("Existing-folder contract does not conflate distinct literal case variants", "[folder-constraint]")
{
    const FolderTreeCatalog::Catalog catalog({{"Finance/Invoices", 2}, {"Finance/invoices", 2}});
    const auto decision = parse_existing(R"({"targetFolder":"Finance/invoices","createFolder":false})", catalog);
    REQUIRE(decision.selection);
    CHECK(decision.selection->relative_path == "Finance/invoices");
}

TEST_CASE("Proposal contract requires an explicit safe path and authoritative existence flags", "[folder-constraint]")
{
    const FolderTreeCatalog::Catalog catalog({{"Finance", 1}, {"Finance/Invoices", 2}});
    const auto schema = QJsonDocument::fromJson(QByteArray::fromStdString(proposal_constraint().json_schema)).object();
    CHECK(schema["properties"].toObject()["createFolder"].toObject()["const"] == true);
    const std::string target = GENERATE("../outside", "/outside", "", "Finance/../Taxes");
    const auto response = QJsonDocument(QJsonObject{{"targetFolder", QString::fromStdString(target)},
        {"createFolder", true}}).toJson(QJsonDocument::Compact).toStdString();
    CHECK_FALSE(parse_proposal(response, catalog));
    CHECK_FALSE(parse_proposal(R"({"targetFolder":null,"createFolder":true})", catalog));
    CHECK_FALSE(parse_proposal(R"({"targetFolder":"Finance/Forecasts","createFolder":false})", catalog));
    const auto selection = parse_proposal(R"({"targetFolder":"Finance/Forecasts","createFolder":true})", catalog);
    REQUIRE(selection);
    CHECK(selection->suggested_new);
    CHECK_FALSE(selection->exists);
}

TEST_CASE("Proposal contract reuses only literal existing targets without fuzzy repair", "[folder-constraint]")
{
    const FolderTreeCatalog::Catalog catalog({{"Finance", 1}, {"Finance/Invoices", 2}});
    const auto selection = parse_proposal(R"({"targetFolder":"Finance/Invoices","createFolder":true})", catalog);
    REQUIRE(selection);
    CHECK(selection->exists);
    CHECK_FALSE(selection->suggested_new);
    CHECK_FALSE(parse_proposal(R"({"targetFolder":"finance/invoices","createFolder":true})", catalog));
    CHECK_FALSE(parse_proposal(R"({"targetFolder":"Finance/Invoices/","createFolder":true})", catalog));
}
