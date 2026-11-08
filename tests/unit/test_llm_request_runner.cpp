#include "ILLMClient.hpp"
#include "ILLMRequestControl.hpp"
#include "LLMRequestRunner.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <atomic>
#include <thread>

namespace {
using namespace std::chrono_literals;
class Client : public ILLMClient {
public:
    std::string categorize_file(const std::string&, const std::string&, FileType, const std::string&) override { return {}; }
    std::string complete_prompt(const std::string&, int) override { return {}; }
    void set_prompt_logging_enabled(bool) override {}
};
class ControlledClient : public Client, public ILLMRequestControl {
public:
    std::optional<Clock::time_point> deadline;
    void set_request_deadline(std::optional<Clock::time_point> value) noexcept override { deadline = value; }
};
}

TEST_CASE("LLMRequestRunner clears deadlines after success and exceptions", "[llm-request]")
{
    ControlledClient client;
    bool deadline_seen = false;
    CHECK(LLMRequestRunner::run(client, [&] { deadline_seen = client.deadline.has_value(); return "ok"; }, 1s) == "ok");
    CHECK(deadline_seen);
    CHECK_FALSE(client.deadline);
    CHECK_THROWS_WITH(LLMRequestRunner::run(client, []() -> std::string {
        throw std::runtime_error("backend failed");
    }, 1s), "backend failed");
    CHECK_FALSE(client.deadline);
}

TEST_CASE("LLMRequestRunner drains noncooperative requests before returning a timeout", "[llm-request]")
{
    Client client;
    std::atomic<bool> finished{false};
    CHECK_THROWS_WITH(LLMRequestRunner::run(client, [&] {
        std::this_thread::sleep_for(30ms);
        finished = true;
        return "late";
    }, 1ms), "Timed out waiting for LLM response");
    CHECK(finished.load());
    CHECK(LLMRequestRunner::run(client, [] { return "next"; }, 1s) == "next");
}

TEST_CASE("LLMRequestRunner propagates cooperative deadlines and waits for cancellation", "[llm-request]")
{
    ControlledClient client;
    bool stopped = false;
    CHECK_THROWS_WITH(LLMRequestRunner::run(client, [&] {
        while (ILLMRequestControl::Clock::now() < *client.deadline) std::this_thread::yield();
        stopped = true;
        return std::string();
    }, 5ms), "Timed out waiting for LLM response");
    CHECK(stopped);
    CHECK_FALSE(client.deadline);
}
