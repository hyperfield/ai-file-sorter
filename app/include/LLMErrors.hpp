#ifndef LLM_ERRORS_HPP
#define LLM_ERRORS_HPP

#include <stdexcept>
#include <string>
#include <cstddef>
#include <optional>

/** @brief The complete chat prompt cannot fit without losing required instructions. */
class PromptBudgetError : public std::runtime_error {
public:
    /**
     * @brief Construct a non-backend error describing the input budget failure.
     * @param message Description of the prompt fitting failure.
     * @param input_budget Actual input token capacity after allocation, when known.
     */
    explicit PromptBudgetError(const std::string& message,
                              std::optional<std::size_t> input_budget = std::nullopt)
        : std::runtime_error(message), input_budget_(input_budget) {}
    /** @brief Actual available input space after context allocation, if known. */
    std::optional<std::size_t> input_budget() const { return input_budget_; }
private:
    std::optional<std::size_t> input_budget_;
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
