#include "NativeJsonSchemaGrammar.hpp"
#include "LLMErrors.hpp"
#include "json-schema-to-grammar.h"

namespace {
common_json ordered_schema(common_json schema)
{
    if (!schema.contains("properties") || !schema.at("properties").is_object() ||
        !schema.contains("required") || !schema.at("required").is_array()) return schema;
    const auto& properties = schema.at("properties");
    auto ordered = common_json::object();
    // Qt sorts object keys. Preserve the contract's required-field sequence for native decoding.
    for (const auto& name : schema.at("required")) {
        if (!name.is_string()) continue;
        const auto key = name.get<std::string>();
        if (properties.contains(key)) ordered[key] = properties.at(key);
    }
    for (const auto& [key, value] : properties.items()) {
        if (!ordered.contains(key)) ordered[key] = value;
    }
    schema["properties"] = std::move(ordered);
    return schema;
}
}

std::string NativeJsonSchemaGrammar::compile(std::string_view schema)
{
    if (schema.empty()) return {};
    try {
        const auto value = common_json::parse(std::string(schema));
        if (!value.is_object() || value.empty()) {
            throw ResponseConstraintError("A response constraint must be a nonempty JSON schema object");
        }
        auto grammar = json_schema_to_grammar(ordered_schema(value), true);
        if (grammar.empty()) throw ResponseConstraintError("Response schema compiled to an empty grammar");
        return grammar;
    } catch (const std::exception& error) {
        throw ResponseConstraintError(std::string("Cannot compile response constraint: ") + error.what());
    }
}
