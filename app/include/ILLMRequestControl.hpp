#pragma once

#include <chrono>
#include <optional>

/** @brief Optional cooperative deadline support, separate from the LLM content API. */
class ILLMRequestControl {
public:
    using Clock = std::chrono::steady_clock;
    virtual ~ILLMRequestControl() = default;

    /**
     * @brief Set the deadline checked by the next request's inference loop.
     * @param deadline Expiry time, or empty to remove the request deadline.
     */
    virtual void set_request_deadline(std::optional<Clock::time_point> deadline) noexcept = 0;
};
