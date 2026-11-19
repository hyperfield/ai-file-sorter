#pragma once

#include <string>

/** @brief Per-request structured output contract, independent of the inference backend. */
struct LLMResponseConstraint {
    /** @brief JSON schema to enforce while decoding, or empty for unconstrained completion. */
    std::string json_schema;
};
