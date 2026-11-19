#pragma once

#include <stdexcept>

/** @brief A source-specific decoding/extraction failure, distinct from runtime or API failures. */
class ContentAnalysisUnavailable : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
