#include "NativeJsonSchemaGrammar.hpp"
#include "FolderTreeSelectionContract.hpp"
#include "LLMErrors.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

TEST_CASE("Native schema compilation is explicit and fails closed", "[native-grammar]")
{
    CHECK(NativeJsonSchemaGrammar::compile("").empty());
    const std::string invalid = GENERATE("not JSON", "null", "[]", "{}", "true");
    CHECK_THROWS_AS(NativeJsonSchemaGrammar::compile(invalid), ResponseConstraintError);
}

TEST_CASE("Native schema compilation supports path enums null and proposal objects", "[native-grammar]")
{
    const FolderTreeCatalog::Catalog catalog({{"70-79 Media & Design/74 Presentations", 2},
        {"Research/Quotes \"and\" caf\xc3\xa9", 2}});
    const auto grammar = NativeJsonSchemaGrammar::compile(
        FolderTreeSelectionContract::existing_constraint(catalog.entries()).json_schema);
    CHECK(grammar.find("root ::=") != std::string::npos);
    CHECK(grammar.find("70-79 Media & Design/74 Presentations") != std::string::npos);
    CHECK(grammar.find("40-79 Media & Design") == std::string::npos);
    CHECK(grammar.find("null") != std::string::npos);
    CHECK(grammar.find("false") != std::string::npos);
    const auto root_start = grammar.find("root ::=");
    const auto root = grammar.substr(root_start, grammar.find('\n', root_start) - root_start);
    CHECK(root.find("targetFolder") < root.find("createFolder"));
    CHECK_FALSE(NativeJsonSchemaGrammar::compile(
        FolderTreeSelectionContract::proposal_constraint().json_schema).empty());
}

TEST_CASE("Native schema field ordering retains optional properties and original constraints", "[native-grammar]")
{
    const auto grammar = NativeJsonSchemaGrammar::compile(R"({"type":"object",
        "properties":{"alpha":{"const":false},"optional":{"type":"string"},"zeta":{"enum":["exact",null]}},
        "required":["zeta","alpha"],"additionalProperties":false})");
    const auto root_start = grammar.find("root ::=");
    REQUIRE(root_start != std::string::npos);
    const auto root = grammar.substr(root_start, grammar.find('\n', root_start) - root_start);
    CHECK(root.find("zeta") < root.find("alpha"));
    CHECK(grammar.find("optional") != std::string::npos);
    CHECK(grammar.find("exact") != std::string::npos);
    CHECK(grammar.find("false") != std::string::npos);
    CHECK(grammar.find("null") != std::string::npos);
}
