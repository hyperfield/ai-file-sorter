#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>

class ILLMClient;
namespace spdlog { class logger; }

/** @brief Keeps a timed request joined before its borrowed client can be reused or destroyed. */
class LLMRequestRunner {
public:
    /**
     * @brief Execute a request with a cooperative deadline and an owned worker.
     * @param client Client borrowed for the entire request, including timeout cleanup.
     * @param request Request operation, with owned copies of its input data.
     * @param timeout Maximum useful response time; non-cooperative clients are drained on expiry.
     * @param logger Optional logger for timeout cleanup.
     * @return Completed response; throws on expiry or a request error.
     */
    static std::string run(ILLMClient& client, std::function<std::string()> request,
                           std::chrono::milliseconds timeout,
                           const std::shared_ptr<spdlog::logger>& logger = {});
};
