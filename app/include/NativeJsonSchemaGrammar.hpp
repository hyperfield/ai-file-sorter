#pragma once

#include <string>
#include <string_view>

/** @brief Convert structured output schemas using the bundled runtime's grammar implementation. */
namespace NativeJsonSchemaGrammar {
/**
 * @brief Compile to native GBNF without loading a model, following required-field order.
 * @param schema Complete JSON schema, or empty to disable the constraint.
 * @return Native grammar text, or empty when no constraint was requested.
 * @throws ResponseConstraintError When a requested schema cannot be compiled.
 */
std::string compile(std::string_view schema);
}
