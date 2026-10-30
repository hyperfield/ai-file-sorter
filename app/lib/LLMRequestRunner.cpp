#include "LLMRequestRunner.hpp"
#include "ILLMClient.hpp"
#include "ILLMRequestControl.hpp"

#include <future>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <utility>

namespace {
class DeadlineScope {
public:
    DeadlineScope(ILLMClient& client, ILLMRequestControl::Clock::time_point deadline)
        : control_(dynamic_cast<ILLMRequestControl*>(&client))
    {
        if (control_) control_->set_request_deadline(deadline);
    }
    ~DeadlineScope()
    {
        if (control_) control_->set_request_deadline(std::nullopt);
    }
private:
    ILLMRequestControl* control_;
};
} // namespace

std::string LLMRequestRunner::run(ILLMClient& client, std::function<std::string()> request,
                                 std::chrono::milliseconds timeout,
                                 const std::shared_ptr<spdlog::logger>& logger)
{
    const auto deadline = ILLMRequestControl::Clock::now() + timeout;
    DeadlineScope scope(client, deadline);
    // The async future joins on every exit, including exceptions. Never detach a borrowed client.
    auto future = std::async(std::launch::async, std::move(request));
    const auto status = future.wait_until(deadline);
    if (status == std::future_status::timeout || ILLMRequestControl::Clock::now() >= deadline) {
        if (logger) logger->warn("LLM request deadline expired; waiting for active inference to stop before reusing its client");
        future.wait();
        throw std::runtime_error("Timed out waiting for LLM response");
    }
    return future.get();
}
