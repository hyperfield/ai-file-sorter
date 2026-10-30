#ifndef LLM_ERRORS_HPP
#define LLM_ERRORS_HPP

#include <stdexcept>
#include <string>

/** @brief The complete chat prompt cannot fit without losing required instructions. */
class PromptBudgetError : public std::runtime_error {
public:
    /** @brief Construct a non-backend error describing the input budget failure. */
    explicit PromptBudgetError(const std::string& message) : std::runtime_error(message) {}
};

/** @brief A requested output contract could not be enforced; never retry unconstrained or on CPU. */
class ResponseConstraintError : public std::runtime_error {
public:
    /** @brief Construct a non-backend constraint error. @param message Failure description. */
    explicit ResponseConstraintError(const std::string& message) : std::runtime_error(message) {}
};

class BackoffError : public std::runtime_error {
public:
    BackoffError(const std::string& message, int retry_seconds)
        : std::runtime_error(message), retry_after_seconds_(retry_seconds) {}

    int retry_after_seconds() const { return retry_after_seconds_; }

private:
    int retry_after_seconds_{0};
};

#endif // LLM_ERRORS_HPP
