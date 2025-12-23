#ifndef LLM_ERRORS_HPP
#define LLM_ERRORS_HPP

#include <stdexcept>
#include <string>

class BackoffError : public std::runtime_error {
public:
    BackoffError(const std::string& message, int retry_seconds)
        : std::runtime_error(message), retry_after_seconds_(retry_seconds) {}

    int retry_after_seconds() const { return retry_after_seconds_; }

private:
    int retry_after_seconds_{0};
};

#endif // LLM_ERRORS_HPP
