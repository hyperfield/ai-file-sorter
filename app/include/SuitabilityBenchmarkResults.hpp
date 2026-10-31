#pragma once

#include "Types.hpp"
#include <string>
#include <vector>

namespace SuitabilityBenchmark {

struct Result {
    std::string model_key;
    std::string model_path;
    std::string visual_id;
    std::string label;
    std::string backend;
    LLMChoice choice{LLMChoice::Unset};
    bool visual{false};
    bool categorization_success{false};
    bool document_success{false};
    bool image_success{false};
    double categorization_seconds{0};
    double document_seconds{0};
    double image_seconds{0};
    std::string detail;
    std::string tested_at;

    bool success() const {
        return visual ? image_success : categorization_success && document_success;
    }
};

using Results = std::vector<Result>;

/** Replace the latest measurement for this pair, preserving every other pair. */
void merge_result(Results& results, Result result);
Results failed_pairs(const Results& results);
std::string serialize_results(const Results& results);
Results deserialize_results(const std::string& json);

} // namespace SuitabilityBenchmark
